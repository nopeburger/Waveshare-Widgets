#pragma once
#include <Arduino.h>
#include <atomic>
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "../../core/services.h"
#include "quake_feed.h"
#include "quake_draw.h"

namespace quake {
class Runtime {
 public:
  Slideshow show;Snapshot view;sky::Home home;
  void begin(deck::Services &shared){
    services=&shared;mutex=xSemaphoreCreateMutex();assert(mutex);
    if(xTaskCreatePinnedToCore(networkTask,"quake-feed",20480,this,1,&worker,0)!=pdPASS)publish(Status::Error,"NETWORK TASK FAILED");
  }
  void tick(uint32_t now,bool active){
    uint32_t revision;home=services->location(revision);
    xSemaphoreTake(mutex,portMAX_DELAY);view=latest;xSemaphoreGive(mutex);
    if(!sameCenter(view,home)){view.count=0;if(view.revision)view.status=Status::Waiting;}
    if(!active)show.switched=now;
    show.update(view,now,int64_t(time(nullptr)));
  }
  void render(ui::Canvas &canvas,uint32_t now){quake::render(canvas,show,view,now,int64_t(time(nullptr)));}
  bool command(const char *line){
    if(!strcmp(line,"quake status")||!strcmp(line,"earthquake status")){
      Serial.printf("earthquakes=%d state=%d index=%d hold=%d radius=100mi window=7d stack_free=%u id=%s\n",show.count,int(view.status),show.count?show.index+1:0,show.held,worker?unsigned(uxTaskGetStackHighWaterMark(worker)):0,show.current()?show.current()->id:"none");return true;
    }
    if(!strcmp(line,"quake refresh")||!strcmp(line,"earthquake refresh")){requested.store(true);return true;}
    return false;
  }
 private:
  struct Body {
    static constexpr size_t Limit=128*1024;
    char *data=nullptr;size_t length=0;bool overflow=false;uint32_t retryAfter=0;
    Body(){data=(char*)heap_caps_malloc(Limit+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(data)data[0]=0;}
    ~Body(){free(data);}
  };
  deck::Services *services=nullptr;SemaphoreHandle_t mutex=nullptr;TaskHandle_t worker=nullptr;
  Snapshot latest;std::atomic<bool> requested{false};
  void publish(Status status,const char *message){xSemaphoreTake(mutex,portMAX_DELAY);latest.status=status;snprintf(latest.message,sizeof(latest.message),"%s",message);xSemaphoreGive(mutex);}
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
  bool fetch(const sky::Home &home,uint32_t revision,uint32_t &retryAfter){
    time_t start=time(nullptr)-WindowSeconds;tm utc={};gmtime_r(&start,&utc);char from[28];strftime(from,sizeof(from),"%Y-%m-%dT%H:%M:%S",&utc);
    char url[384];snprintf(url,sizeof(url),"https://earthquake.usgs.gov/fdsnws/event/1/query?format=geojson&latitude=%.7f&longitude=%.7f&maxradiuskm=160.9344&starttime=%s&orderby=time&eventtype=earthquake&limit=%d",home.lat,home.lon,from,MaxEvents);
    Body body;if(!body.data)return false;
    esp_http_client_config_t config={};config.url=url;config.crt_bundle_attach=esp_crt_bundle_attach;config.timeout_ms=15000;
    config.event_handler=onHttp;config.user_data=&body;config.disable_auto_redirect=true;config.buffer_size=4096;
    esp_http_client_handle_t client=esp_http_client_init(&config);if(!client)return false;
    esp_http_client_set_header(client,"Accept","application/json");esp_http_client_set_header(client,"Accept-Encoding","identity");esp_http_client_set_header(client,"User-Agent","WidgetDeck-Earthquakes/1.0");
    esp_err_t result=esp_http_client_perform(client);int code=esp_http_client_get_status_code(client);bool complete=esp_http_client_is_complete_data_received(client);
    retryAfter=body.retryAfter;esp_http_client_cleanup(client);
    Snapshot parsed;bool valid=false;
    if(result==ESP_OK&&!body.overflow){
      if(code==204){parsed.lat=home.lat;parsed.lon=home.lon;parsed.generated=time(nullptr);parsed.status=Status::Live;valid=true;}
      else if(code==200&&complete)valid=parseFeed(body.data,body.length,home,int64_t(time(nullptr)),parsed);
    }
    if(!valid){Serial.printf("USGS fetch failed: transport=%d HTTP=%d complete=%d overflow=%d. Retaining cached reports.\n",int(result),code,complete,body.overflow);return false;}
    uint32_t currentRevision;services->location(currentRevision);if(currentRevision!=revision)return false;
    xSemaphoreTake(mutex,portMAX_DELAY);parsed.revision=latest.revision+1;latest=parsed;xSemaphoreGive(mutex);
    Serial.printf("USGS: %d recent earthquakes within 100 miles, %u bytes.\n",parsed.count,unsigned(body.length));return true;
  }
  static void networkTask(void *arg){static_cast<Runtime*>(arg)->run();}
  void run(){
    uint32_t due=0,lastAttempt=0,lastRevision=0,backoff=30000,retryUntil=0;bool attempted=false;Status online=Status::Waiting;
    for(;;){
      uint32_t revision;sky::Home h=services->location(revision);uint32_t now=millis();
      if(revision!=lastRevision){due=0;lastRevision=revision;online=Status::Waiting;xSemaphoreTake(mutex,portMAX_DELAY);latest.count=0;latest.lat=NAN;latest.lon=NAN;latest.generated=0;latest.revision=0;latest.status=Status::Waiting;xSemaphoreGive(mutex);}
      if(WiFi.status()!=WL_CONNECTED){publish(Status::Offline,"WI-FI OFFLINE");vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(time(nullptr)<1700000000){publish(Status::Clock,"WAITING FOR NETWORK TIME");vTaskDelay(pdMS_TO_TICKS(500));continue;}
      publish(online,online==Status::Live?"LIVE":online==Status::Error?"RETRYING USGS":"CONNECTING TO USGS");
      bool cooldown=retryUntil&&int32_t(now-retryUntil)<0;
      bool manual=requested.load()&&(!attempted||uint32_t(now-lastAttempt)>=60000)&&!cooldown;
      if(cooldown||(due&&int32_t(now-due)<0&&!manual)){vTaskDelay(pdMS_TO_TICKS(250));continue;}
      requested.store(false);attempted=true;lastAttempt=now;uint32_t retryAfter=0;
      xSemaphoreTake(services->networkMutex,portMAX_DELAY);bool success=fetch(h,revision,retryAfter);xSemaphoreGive(services->networkMutex);
      online=success?Status::Live:Status::Error;retryUntil=retryAfter?millis()+retryAfter:0;
      if(success){backoff=30000;due=millis()+RefreshMs;}
      else{due=millis()+std::max(backoff,retryAfter);backoff=std::min(backoff*2,uint32_t(15*60*1000));}
    }
  }
};
}
