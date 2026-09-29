#pragma once
#include <Arduino.h>
#include <atomic>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "../../core/services.h"
#include "space_feed.h"
#include "space_draw.h"

namespace spacewx {
class Runtime {
 public:
  Snapshot view;unsigned page=0;
  void begin(deck::Services &shared){
    services=&shared;mutex=xSemaphoreCreateMutex();assert(mutex);
    if(xTaskCreatePinnedToCore(networkTask,"space-feed",12288,this,1,&worker,0)!=pdPASS)setLinks(Link::Error);
  }
  void tick(uint32_t now,bool active){xSemaphoreTake(mutex,portMAX_DELAY);view=latest;xSemaphoreGive(mutex);}
  void render(ui::Canvas &c,uint32_t now){spacewx::render(c,view,page,now,int64_t(time(nullptr)));}
  void refresh(){requested.store(true);}
  bool command(const char *line){
    if(!strcmp(line,"space refresh")){refresh();return true;}
    if(!strcmp(line,"space status")){
      int64_t epoch=time(nullptr);
      Serial.printf("space page=%u links=%d/%d/%d current=%d/%d/%d G=%d S=%d R=%d kp=%.2f wind_mph=%.0f samples=%d revision=%u stack_free=%u\n",page+1,int(view.links[0]),int(view.links[1]),int(view.links[2]),scalesFresh(view,epoch),kpFresh(view,epoch),windFresh(view,epoch),view.scales.g,view.scales.s,view.scales.r,double(view.kp.value),double(view.wind.mph),view.kp.count,view.revision,worker?unsigned(uxTaskGetStackHighWaterMark(worker)):0);return true;
    }
    return false;
  }
 private:
  struct Body {
    static constexpr size_t Limit=64*1024;
    char *data=nullptr;size_t length=0;bool overflow=false;uint32_t retryAfter=0;
    Body(){data=(char*)heap_caps_malloc(Limit+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(data)data[0]=0;}
    ~Body(){free(data);}
  };
  deck::Services *services=nullptr;SemaphoreHandle_t mutex=nullptr;TaskHandle_t worker=nullptr;
  Snapshot latest;std::atomic<bool> requested{false};
  void setLinks(Link link){xSemaphoreTake(mutex,portMAX_DELAY);for(auto &state:latest.links)state=link;xSemaphoreGive(mutex);}
  void setLink(int source,Link link){xSemaphoreTake(mutex,portMAX_DELAY);latest.links[source]=link;xSemaphoreGive(mutex);}
  static esp_err_t onHttp(esp_http_client_event_t *event){
    Body &body=*static_cast<Body*>(event->user_data);
    if(event->event_id==HTTP_EVENT_ON_HEADER&&event->header_key&&event->header_value&&!strcasecmp(event->header_key,"Retry-After")){
      char *end=nullptr;long seconds=strtol(event->header_value,&end,10);if(end!=event->header_value&&*end==0&&seconds>0)body.retryAfter=uint32_t(std::min(seconds,3600L))*1000;
    }
    if(event->event_id==HTTP_EVENT_ON_DATA&&event->data_len>0){
      if(!body.data||size_t(event->data_len)>Body::Limit-body.length){body.overflow=true;return ESP_FAIL;}
      memcpy(body.data+body.length,event->data,event->data_len);body.length+=event->data_len;body.data[body.length]=0;vTaskDelay(1);
    }
    return ESP_OK;
  }
  bool fetch(int source,uint32_t &retryAfter){
    static const char *urls[]={"https://services.swpc.noaa.gov/products/noaa-scales.json","https://services.swpc.noaa.gov/json/planetary_k_index_1m.json","https://services.swpc.noaa.gov/products/summary/solar-wind-speed.json"};
    Body body;if(!body.data)return false;
    esp_http_client_config_t config={};config.url=urls[source];config.crt_bundle_attach=esp_crt_bundle_attach;config.timeout_ms=10000;
    config.event_handler=onHttp;config.user_data=&body;config.disable_auto_redirect=true;config.buffer_size=2048;
    esp_http_client_handle_t client=esp_http_client_init(&config);if(!client)return false;
    esp_http_client_set_header(client,"Accept","application/json");esp_http_client_set_header(client,"Accept-Encoding","identity");esp_http_client_set_header(client,"User-Agent","WidgetDeck-SpaceWeather/1.0");
    esp_err_t result=esp_http_client_perform(client);int code=esp_http_client_get_status_code(client);bool complete=esp_http_client_is_complete_data_received(client);
    retryAfter=body.retryAfter;esp_http_client_cleanup(client);
    bool valid=false;Scales scales;Kp kp;Wind wind;int64_t epoch=time(nullptr);
    if(result==ESP_OK&&code==200&&complete&&!body.overflow){
      if(source==0)valid=parseScales(body.data,body.length,epoch,scales);
      else if(source==1)valid=parseKp(body.data,body.length,epoch,kp);
      else valid=parseWind(body.data,body.length,epoch,wind);
    }
    if(!valid){Serial.printf("NOAA source=%d failed: transport=%d HTTP=%d complete=%d overflow=%d\n",source,int(result),code,complete,body.overflow);return false;}
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(source==0)latest.scales=scales;else if(source==1)latest.kp=kp;else latest.wind=wind;
    latest.links[source]=Link::Live;++latest.revision;xSemaphoreGive(mutex);
    Serial.printf("NOAA source=%d: %u bytes, observation accepted.\n",source,unsigned(body.length));return true;
  }
  static void networkTask(void *arg){static_cast<Runtime*>(arg)->run();}
  void run(){
    uint32_t due[3]={},lastAttempt[3]={},backoff[3]={30000,30000,30000},retryUntil[3]={};bool attempted[3]={},wasOffline=false;
    for(;;){
      uint32_t now=millis();
      if(WiFi.status()!=WL_CONNECTED){setLinks(Link::Offline);wasOffline=true;vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(time(nullptr)<1700000000){setLinks(Link::Clock);vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(wasOffline){for(auto &time:due)time=0;wasOffline=false;}
      bool manual=requested.load(),consumed=false;
      for(int source=0;source<3;source++){
        now=millis();bool cooldown=retryUntil[source]&&int32_t(now-retryUntil[source])<0;
        bool userDue=manual&&(!attempted[source]||uint32_t(now-lastAttempt[source])>=60000);
        if(cooldown||(due[source]&&int32_t(now-due[source])<0&&!userDue))continue;
        if(WiFi.status()!=WL_CONNECTED)break;
        attempted[source]=true;lastAttempt[source]=now;uint32_t retryAfter=0;
        // Yield the network lock between sources so other widgets can update.
        xSemaphoreTake(services->networkMutex,portMAX_DELAY);bool success=fetch(source,retryAfter);xSemaphoreGive(services->networkMutex);
        retryUntil[source]=retryAfter?millis()+retryAfter:0;
        if(success){backoff[source]=30000;due[source]=millis()+RefreshMs;}
        else{setLink(source,Link::Error);due[source]=millis()+std::max(backoff[source],retryAfter);backoff[source]=std::min(backoff[source]*2,uint32_t(10*60*1000));}
        consumed=true;vTaskDelay(pdMS_TO_TICKS(30));
      }
      if(consumed&&manual)requested.store(false);
      vTaskDelay(pdMS_TO_TICKS(250));
    }
  }
};
}
