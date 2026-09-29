#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include <time.h>
#include <atomic>
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "gas_feed.h"
#include "gas_seed.h"
#include "../../core/services.h"
#include "../../../config.h"

namespace gas {
class Runtime {
 public:
  void begin(deck::Services &shared) {
    services=&shared;mutex=xSemaphoreCreateMutex();assert(mutex);settings.begin("gas-price",false);loadCache();
    if(!view.history.count){view.history=initialHistory();view.state=State::Cached;}
    if(!strlen(GAS_EIA_API_KEY)){setState(State::MissingKey);return;}
    if(xTaskCreatePinnedToCore(networkTask,"gas-feed",16384,this,1,nullptr,0)!=pdPASS)setState(State::Error);
  }
  View snapshot(){xSemaphoreTake(mutex,portMAX_DELAY);View copy=view;xSemaphoreGive(mutex);time_t now=time(nullptr);if(now>1700000000)copy.today=int(now/86400);return copy;}
  void refresh(){requested.store(true);}
 private:
  struct Cache {uint32_t magic=0x47415331;History history;};
  struct Body {static constexpr size_t Limit=128*1024;char *data=nullptr;size_t size=0;bool overflow=false;Body(){data=(char*)heap_caps_malloc(Limit+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}~Body(){free(data);}};
  deck::Services *services=nullptr;
  Preferences settings;SemaphoreHandle_t mutex=nullptr;View view;std::atomic<bool> requested{false};
  static void *jsonAllocate(size_t size){return heap_caps_malloc(size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
  void setState(State s){xSemaphoreTake(mutex,portMAX_DELAY);view.state=s;xSemaphoreGive(mutex);}
  void loadCache() {
    Cache c;
    if(settings.getBytesLength("history")==sizeof(c)&&settings.getBytes("history",&c,sizeof(c))==sizeof(c)&&c.magic==0x47415331&&normalize(c.history,dayNumber({2100,12,31}))){view.history=c.history;view.state=State::Cached;}
  }
  static esp_err_t onHttp(esp_http_client_event_t *event) {
    Body *body=static_cast<Body*>(event->user_data);
    if(event->event_id==HTTP_EVENT_ON_DATA&&event->data_len>0){
      if(!body->data||size_t(event->data_len)>Body::Limit-body->size){body->overflow=true;return ESP_FAIL;}
      memcpy(body->data+body->size,event->data,event->data_len);body->size+=event->data_len;body->data[body->size]=0;
    }
    return ESP_OK;
  }
  bool fetch() {
    // Never log this URL: EIA requires the key in the query string.
    String url="https://api.eia.gov/v2/seriesid/PET.EMM_EPMR_PTE_SCA_DPG.W?api_key=";
    url+=GAS_EIA_API_KEY;url+="&sort%5B0%5D%5Bcolumn%5D=period&sort%5B0%5D%5Bdirection%5D=desc&length=200";
    Body body;if(!body.data)return false;
    esp_http_client_config_t config={};config.url=url.c_str();config.crt_bundle_attach=esp_crt_bundle_attach;
    config.timeout_ms=15000;config.event_handler=onHttp;config.user_data=&body;config.disable_auto_redirect=true;config.buffer_size=4096;
    esp_http_client_handle_t client=esp_http_client_init(&config);if(!client)return false;
    esp_http_client_set_header(client,"Accept","application/json");esp_http_client_set_header(client,"Accept-Encoding","identity");
    esp_http_client_set_header(client,"User-Agent","CaliforniaGas-ESP32/1.0");
    esp_err_t result=esp_http_client_perform(client);int status=esp_http_client_get_status_code(client);
    bool complete=esp_http_client_is_complete_data_received(client);esp_http_client_cleanup(client);
    History parsed;
    if(result!=ESP_OK||status!=200||!complete||body.overflow||!parseFeed(body.data,body.size,int(time(nullptr)/86400),parsed)){Serial.printf("EIA fetch failed: transport=%d HTTP=%d. Retaining cached data.\n",int(result),status);return false;}
    xSemaphoreTake(mutex,portMAX_DELAY);
    bool changed=parsed.count!=view.history.count;
    if(!changed)for(unsigned i=0;i<parsed.count;i++)if(parsed.points[i].day!=view.history.points[i].day||parsed.points[i].price!=view.history.points[i].price){changed=true;break;}
    view.history=parsed;view.state=State::Current;xSemaphoreGive(mutex);
    if(changed){Cache cache;cache.history=parsed;settings.putBytes("history",&cache,sizeof(cache));}
    char date[24];dateLabel(parsed.latest().day,date,sizeof(date));Serial.printf("EIA: %u weekly points, latest %.3f USD/gal (%s).\n",unsigned(parsed.count),double(parsed.latest().price),date);return true;
  }
  static void networkTask(void *arg){static_cast<Runtime*>(arg)->run();}
  void run() {
    uint32_t due=0,lastAttempt=0,backoff=30000;bool attempted=false;
    xSemaphoreTake(mutex,portMAX_DELAY);State onlineState=view.state;xSemaphoreGive(mutex);
    for(;;) {
      uint32_t now=millis();
      if(WiFi.status()!=WL_CONNECTED){setState(State::Offline);vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(time(nullptr)<1700000000){setState(State::Clock);vTaskDelay(pdMS_TO_TICKS(500));continue;}
      // A connection interruption must not leave OFFLINE showing until the next
      // six-hour refresh. Restore the last fetch result without bypassing its schedule.
      setState(onlineState);
      // Manual refresh has a 60-second floor to protect the API quota.
      bool manual=requested.load()&&(!attempted||now-lastAttempt>=60000);
      bool pending=due&&int32_t(now-due)<0;
      if(pending&&!manual){vTaskDelay(pdMS_TO_TICKS(200));continue;}
      requested.store(false);lastAttempt=now;attempted=true;
      xSemaphoreTake(services->networkMutex,portMAX_DELAY);bool success=fetch();xSemaphoreGive(services->networkMutex);
      onlineState=success?State::Current:State::Error;
      if(success){backoff=30000;due=millis()+GAS_REFRESH_MS;}
      else {setState(State::Error);due=millis()+backoff;backoff=std::min(backoff*2,uint32_t(30*60*1000));}
    }
  }
};
}
