#pragma once
#include <Arduino.h>
#include <atomic>
#include "mqtt_client.h"
#include "../../core/services.h"
#include "printer_settings.h"
#include "printer_feed.h"
#include "printer_draw.h"
#include "printer_trust.h"
#include "printer_camera.h"
namespace printer {
class Runtime {
 public:
  Snapshot view;unsigned page=0;
  void begin(deck::Services &shared){
    services=&shared;mutex=xSemaphoreCreateMutex();assert(mutex);
    if(!loadSettings())return;
    camera.begin(shared,settings);
    snprintf(reportTopic,sizeof(reportTopic),"device/%s/report",settings.serial);
    snprintf(requestTopic,sizeof(requestTopic),"device/%s/request",settings.serial);
    snprintf(clientId,sizeof(clientId),"WidgetDeck-%08X",unsigned(ESP.getEfuseMac()));
    body=(char*)heap_caps_malloc(MessageLimit+1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);message=Message(body,body?MessageLimit+1:0);
    if(!body||xTaskCreatePinnedToCore(networkTask,"printer-feed",6144,this,1,&worker,0)!=pdPASS){publish(Link::Error);Serial.println("Printer: worker/buffer allocation failed.");}
  }
  void tick(uint32_t,bool active){xSemaphoreTake(mutex,portMAX_DELAY);view=latest;xSemaphoreGive(mutex);camera.tick(active&&page==2&&!services->setupVisible);}
  void render(ui::Canvas &canvas,uint32_t now){if(page==2)camera.render(canvas,view,now);else printer::render(canvas,view,page,now,int64_t(time(nullptr)));}
  bool command(const char *line){
    if(!strcmp(line,"printer camera")){camera.status();return true;}
    if(!strcmp(line,"printer status")){Serial.printf("printer link=%d state=%d live=%d progress=%d remaining=%d layer=%d/%d revision=%u page=%u sd=%d worker_free=%u mqtt_free=%u\n",int(view.link),int(view.state),fresh(view,millis()),view.progress,view.remaining,view.layer,view.totalLayers,view.revision,page+1,sdState,worker?unsigned(uxTaskGetStackHighWaterMark(worker)):0,mqttStack.load());return true;}
    if(!strcmp(line,"printer refresh")){requested=true;Serial.println("Printer: full status refresh queued for the next allowed five-minute slot.");return true;}
    return false;
  }
 private:
  Camera camera;
  deck::Services *services=nullptr;SemaphoreHandle_t mutex=nullptr;TaskHandle_t worker=nullptr;
  Settings settings;Snapshot latest;esp_mqtt_client_handle_t client=nullptr;
  char reportTopic[64]={},requestTopic[64]={},clientId[40]={};char *body=nullptr;Message message{nullptr,0};int sdState=0;
  std::atomic<bool> connected{false},broken{false},subscribed{false},requested{false};std::atomic<unsigned> mqttStack{0};
  void publish(Link link){xSemaphoreTake(mutex,portMAX_DELAY);latest.link=link;xSemaphoreGive(mutex);}
  bool loadSettings(){
    Preferences prefs;prefs.begin("deck-printer",false);settings.enabled=prefs.getBool("enabled",true);
    snprintf(settings.ip,sizeof(settings.ip),"%s",prefs.getString("ip","").c_str());
    snprintf(settings.serial,sizeof(settings.serial),"%s",prefs.getString("serial","").c_str());
    snprintf(settings.accessCode,sizeof(settings.accessCode),"%s",prefs.getString("access","").c_str());
    char raw[2049]={};size_t size=0;Settings card;
    if(sky::readCardFile("printer.txt",raw,sizeof(raw),size,sdState)){
      if(!parseSettings(raw,size,card)){sdState=2;publish(Link::InvalidConfig);}
      else{
        if(strcmp(card.ip,settings.ip))prefs.putString("ip",card.ip);
        if(strcmp(card.serial,settings.serial))prefs.putString("serial",card.serial);
        if(strcmp(card.accessCode,settings.accessCode))prefs.putString("access",card.accessCode);
        if(card.enabled!=settings.enabled)prefs.putBool("enabled",card.enabled);
        settings=card;Serial.println("Printer: SD settings loaded (credentials hidden).");
      }
    }
    memset(raw,0,sizeof(raw));memset(&card,0,sizeof(card));prefs.end();
    if(sdState==2){publish(Link::InvalidConfig);return false;}
    if(!settings.enabled){publish(Link::Disabled);return false;}
    if(!validSettings(settings)){publish(Link::Setup);return false;}
    publish(Link::Connecting);return true;
  }
  static void event(void *arg,esp_event_base_t,int32_t,void *data){static_cast<Runtime*>(arg)->onEvent(static_cast<esp_mqtt_event_handle_t>(data));}
  void onEvent(esp_mqtt_event_handle_t e){
    mqttStack.store(unsigned(uxTaskGetStackHighWaterMark(nullptr)));
    switch(e->event_id){
      case MQTT_EVENT_CONNECTED:{
        // Start a new state accumulator: data from before a disconnect must not become live again.
        xSemaphoreTake(mutex,portMAX_DELAY);latest=Snapshot();latest.link=Link::Waiting;xSemaphoreGive(mutex);
        connected=true;int id=esp_mqtt_client_subscribe(e->client,reportTopic,0);
        if(id<0){publish(Link::Error);broken=true;}
        Serial.println("Printer: encrypted local MQTT connected.");break;
      }
      case MQTT_EVENT_SUBSCRIBED:
        if(e->error_handle&&e->error_handle->error_type==MQTT_ERROR_TYPE_SUBSCRIBE_FAILED){publish(Link::Auth);broken=true;}else subscribed=true;break;
      case MQTT_EVENT_DISCONNECTED:
        connected=false;subscribed=false;broken=true;
        xSemaphoreTake(mutex,portMAX_DELAY);if(latest.link!=Link::Auth&&latest.link!=Link::Certificate&&latest.link!=Link::Error)latest.link=Link::Offline;xSemaphoreGive(mutex);break;
      case MQTT_EVENT_ERROR:{
        Link reason=Link::Error;
        if(e->error_handle){const auto &error=*e->error_handle;
          if(error.error_type==MQTT_ERROR_TYPE_CONNECTION_REFUSED&&(error.connect_return_code==MQTT_CONNECTION_REFUSE_BAD_USERNAME||error.connect_return_code==MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED))reason=Link::Auth;
          if(error.error_type==MQTT_ERROR_TYPE_TCP_TRANSPORT&&error.esp_tls_cert_verify_flags)reason=Link::Certificate;
          Serial.printf("Printer MQTT error: type=%d return=%d TLS=%d flags=%d socket=%d.\n",int(error.error_type),int(error.connect_return_code),error.esp_tls_stack_err,error.esp_tls_cert_verify_flags,error.esp_transport_sock_errno);
        }
        publish(reason);broken=true;break;
      }
      case MQTT_EVENT_DATA:{
        if(e->retain){message.reset();break;}
        bool topic=e->topic&&e->topic_len==int(strlen(reportTopic))&&!memcmp(e->topic,reportTopic,size_t(e->topic_len));
        if(message.append(e->data,e->data_len,e->current_data_offset,e->total_data_len,topic)){
          Snapshot next;xSemaphoreTake(mutex,portMAX_DELAY);next=latest;xSemaphoreGive(mutex);
          if(mergeReport(body,message.length,next,millis(),int64_t(time(nullptr)))){
            next.link=Link::Live;xSemaphoreTake(mutex,portMAX_DELAY);latest=next;xSemaphoreGive(mutex);
          }
          message.reset();
        }
        vTaskDelay(1);break;
      }
      default:break;
    }
  }
  bool start(){
    connected=false;broken=false;subscribed=false;message.reset();publish(Link::Connecting);
    esp_mqtt_client_config_t config={};config.broker.address.hostname=settings.ip;config.broker.address.port=8883;config.broker.address.transport=MQTT_TRANSPORT_OVER_SSL;
    config.broker.verification.certificate=BambuCA;config.broker.verification.common_name=settings.serial;
    config.credentials.username="bblp";config.credentials.authentication.password=settings.accessCode;config.credentials.client_id=clientId;
    config.session.protocol_ver=MQTT_PROTOCOL_V_3_1_1;config.session.keepalive=30;
    config.network.disable_auto_reconnect=true;config.network.timeout_ms=10000;
    config.task.priority=1;config.task.stack_size=12288;config.buffer.size=2048;config.buffer.out_size=1024;config.outbox.limit=4096;
    client=esp_mqtt_client_init(&config);
    if(!client||esp_mqtt_client_register_event(client,MQTT_EVENT_ANY,event,this)!=ESP_OK||esp_mqtt_client_start(client)!=ESP_OK){publish(Link::Error);return false;}
    uint32_t began=millis();
    while(!subscribed&&!broken&&WiFi.status()==WL_CONNECTED&&uint32_t(millis()-began)<20000)vTaskDelay(pdMS_TO_TICKS(50));
    bool ok=subscribed&&!broken;
    if(!ok&&!broken)publish(Link::Offline);
    return ok;
  }
  void stop(){if(client){esp_mqtt_client_destroy(client);client=nullptr;}connected=false;subscribed=false;message.reset();}
  static void networkTask(void *arg){static_cast<Runtime*>(arg)->run();}
  void run(){
    uint32_t retryAt=0,backoff=15000,lastPush=0;bool pushed=false;
    for(;;){uint32_t now=millis();
      if(WiFi.status()!=WL_CONNECTED){stop();publish(Link::Wifi);retryAt=0;vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(time(nullptr)<1700000000){publish(Link::Clock);vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(client&&broken){stop();retryAt=millis()+backoff;backoff=std::min(backoff*2,uint32_t(300000));}
      if(!client){
        if(retryAt&&int32_t(now-retryAt)<0){vTaskDelay(pdMS_TO_TICKS(250));continue;}
        // Serialize the expensive TLS handshake with the public feeds, then release the mutex.
        xSemaphoreTake(services->networkMutex,portMAX_DELAY);bool ok=start();xSemaphoreGive(services->networkMutex);
        if(!ok){stop();retryAt=millis()+backoff;backoff=std::min(backoff*2,uint32_t(300000));vTaskDelay(pdMS_TO_TICKS(250));continue;}
        backoff=15000;retryAt=0;
      }
      now=millis();
      if(subscribed&&(!pushed||uint32_t(now-lastPush)>=PushIntervalMs)){
        // Read-only status request; never publish motion, temperature or print-control commands.
        static const char request[]="{\"pushing\":{\"sequence_id\":\"0\",\"command\":\"pushall\",\"version\":1,\"push_target\":1}}";
        int id=esp_mqtt_client_publish(client,requestTopic,request,0,0,0);
        if(id<0){publish(Link::Error);broken=true;}else Serial.println("Printer: requested full status (five-minute minimum interval).");
        pushed=true;lastPush=now;requested=false;
      }
      vTaskDelay(pdMS_TO_TICKS(250));
    }
  }
};
}
