#pragma once
#include <Arduino.h>
#include "esp_http_client.h"
namespace sky {
// Blocking IDF socket reads yield to touch and idle tasks. Response bodies and
// JSON trees live in PSRAM; local HTTP never waits on the shared TLS mutex.
class Body {
 public:
  static constexpr size_t Limit=128*1024;
  char *data=nullptr;size_t length=0,capacity=Limit;bool overflow=false;uint32_t retryAfter=0;
  explicit Body(size_t cap=Limit):capacity(std::min(cap,Limit)){data=(char*)heap_caps_malloc(capacity+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(data)data[0]=0;}
  ~Body(){free(data);}
  Body(const Body&)=delete;Body& operator=(const Body&)=delete;
  static esp_err_t receive(esp_http_client_event_t *event){
    Body &body=*static_cast<Body*>(event->user_data);
    if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&!strcasecmp(event->header_key,"Retry-After")){
      char *end=nullptr;long seconds=strtol(event->header_value,&end,10);
      if(end!=event->header_value&&*end==0&&seconds>0)body.retryAfter=uint32_t(std::min(seconds,3600L))*1000;
    }
    if(event->event_id==HTTP_EVENT_ON_DATA&&event->data_len>0){
      if(!body.data||size_t(event->data_len)>body.capacity-body.length){body.overflow=true;return ESP_FAIL;}
      memcpy(body.data+body.length,event->data,event->data_len);body.length+=event->data_len;body.data[body.length]=0;vTaskDelay(1);
    }
    return ESP_OK;
  }
};
inline int localRequest(const char *url,Body &body,const char *bearer=nullptr,const char *post=nullptr){
  esp_http_client_config_t config={};config.url=url;config.timeout_ms=2500;
  config.event_handler=Body::receive;config.user_data=&body;config.disable_auto_redirect=true;config.buffer_size=4096;
  esp_http_client_handle_t client=body.data?esp_http_client_init(&config):nullptr;if(!client)return 0;
  esp_http_client_set_header(client,"Accept","application/json");esp_http_client_set_header(client,"Accept-Encoding","identity");
  esp_http_client_set_header(client,"Cache-Control","no-cache");esp_http_client_set_header(client,"User-Agent","WidgetDeck/1.0");
  if(bearer&&bearer[0])esp_http_client_set_header(client,"Authorization",bearer);
  if(post){
    esp_http_client_set_method(client,HTTP_METHOD_POST);esp_http_client_set_header(client,"Content-Type","application/json");
    esp_http_client_set_header(client,"X-Meshpoint-Client","widgetdeck");esp_http_client_set_post_field(client,post,strlen(post));
  }
  esp_err_t result=esp_http_client_perform(client);int code=esp_http_client_get_status_code(client);
  bool complete=esp_http_client_is_complete_data_received(client);esp_http_client_cleanup(client);
  return result==ESP_OK&&complete&&!body.overflow?code:0;
}
}
