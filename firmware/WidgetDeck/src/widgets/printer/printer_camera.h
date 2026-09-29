#pragma once
#include <atomic>
#include "esp_tls.h"
#include "jpeg_decoder.h"
#include "../../core/services.h"
#include "printer_settings.h"
#include "printer_trust.h"
#include "printer_camera_draw.h"
namespace printer {
class Camera {
 public:
  void begin(deck::Services &shared,const Settings &config){
    services=&shared;settings=config;mutex=xSemaphoreCreateMutex();assert(mutex);
    if(xTaskCreatePinnedToCore(task,"printer-camera",8192,this,1,&worker,0)!=pdPASS){state(CameraState::Memory);Serial.println("Printer camera: worker allocation failed.");}
  }
  void tick(bool active){visible.store(active);}
  void render(ui::Canvas &c,const Snapshot &s,uint32_t now){
    if(!mutex){CameraView empty;empty.state=CameraState::Config;renderCamera(c,s,empty,now);return;}
    // Hold only during drawing so the worker cannot reuse a frame being displayed.
    xSemaphoreTake(mutex,portMAX_DELAY);renderCamera(c,s,view,now);xSemaphoreGive(mutex);
  }
  void status(){
    if(!mutex){Serial.println("camera: printer configuration required");return;}
    xSemaphoreTake(mutex,portMAX_DELAY);Serial.printf("camera state=%d visible=%d frames=%u age_ms=%u size=%dx%d jpeg_bytes=%u decode_ms=%u interval_ms=%u stream_open=%d connections=%u received_frames=%u worker_free=%u\n",int(view.state),visible.load(),view.revision,view.revision?unsigned(millis()-view.received):0,view.width,view.height,jpegBytes,decodeMs,intervalMs,streamOpen,connections,receivedFrames,worker?unsigned(uxTaskGetStackHighWaterMark(worker)):0);xSemaphoreGive(mutex);
  }
 private:
  deck::Services *services=nullptr;Settings settings;SemaphoreHandle_t mutex=nullptr;TaskHandle_t worker=nullptr;
  std::atomic<bool> visible{false};CameraView view;uint8_t *jpeg=nullptr,*incoming=nullptr,*scratch=nullptr;uint16_t *decoded=nullptr,*front=nullptr,*back=nullptr;
  unsigned jpegBytes=0,decodeMs=0,intervalMs=0,connections=0,receivedFrames=0;
  bool streamOpen=false,pendingImage=false;uint32_t imageReceived=0,lastPublished=0,frameStarted=0;size_t imageLength=0;
  esp_tls_t *tls=nullptr;CameraFrameReader reader;CameraSchedule schedule;CameraRetry retry;
  static constexpr size_t DecodedLimit=480*270*2,FrameBytes=CameraWidth*CameraHeight*2;
  void state(CameraState next){xSemaphoreTake(mutex,portMAX_DELAY);view.state=next;xSemaphoreGive(mutex);}
  void cancelled(){xSemaphoreTake(mutex,portMAX_DELAY);view.state=view.revision?CameraState::Current:CameraState::Waiting;xSemaphoreGive(mutex);}
  bool allocate(){
    if(jpeg)return true;
    jpeg=(uint8_t*)heap_caps_malloc(CameraJpegLimit,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    incoming=(uint8_t*)heap_caps_malloc(CameraJpegLimit,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    scratch=(uint8_t*)heap_caps_malloc(65536,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    decoded=(uint16_t*)heap_caps_malloc(DecodedLimit,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    front=(uint16_t*)heap_caps_malloc(FrameBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);back=(uint16_t*)heap_caps_malloc(FrameBytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(jpeg&&incoming&&scratch&&decoded&&front&&back)return true;
    free(jpeg);free(incoming);free(scratch);free(decoded);free(front);free(back);jpeg=incoming=scratch=nullptr;decoded=front=back=nullptr;state(CameraState::Memory);return false;
  }
  bool wanted()const{return visible.load()&&WiFi.status()==WL_CONNECTED;}
  void close(){
    if(tls){esp_tls_conn_destroy(tls);tls=nullptr;}
    pendingImage=false;reader.reset(incoming,CameraJpegLimit);schedule=CameraSchedule();
    xSemaphoreTake(mutex,portMAX_DELAY);streamOpen=false;xSemaphoreGive(mutex);
  }
  int transfer(uint8_t *bytes,size_t length,bool write,uint32_t started){
    while(wanted()&&uint32_t(millis()-started)<CameraFrameTimeoutMs){
      int n=int(write?esp_tls_conn_write(tls,bytes,length):esp_tls_conn_read(tls,bytes,length));
      if(n==ESP_TLS_ERR_SSL_WANT_READ||n==ESP_TLS_ERR_SSL_WANT_WRITE){vTaskDelay(pdMS_TO_TICKS(10));continue;}
      return n;
    }return -1;
  }
  bool connect(){
    state(CameraState::Capturing);uint32_t started=millis();
    esp_tls_cfg_t config={};config.cacert_buf=(const unsigned char*)BambuCA;config.cacert_bytes=strlen(BambuCA)+1;config.common_name=settings.serial;config.timeout_ms=8000;config.non_block=true;
    tls=esp_tls_init();if(!tls){state(CameraState::Memory);return false;}
    int connected=0;
    // Yield between nonblocking handshake steps; the synchronous helper busy-polls.
    while(wanted()&&uint32_t(millis()-started)<8000){
      connected=esp_tls_conn_new_async(settings.ip,strlen(settings.ip),6000,&config,tls);
      if(connected!=0)break;
      vTaskDelay(pdMS_TO_TICKS(10));
    }
    if(!visible.load()){cancelled();return false;}
    if(connected!=1){
      esp_tls_error_handle_t errors=nullptr;int code=0,flags=0;esp_tls_get_error_handle(tls,&errors);if(errors)esp_tls_get_and_clear_last_error(errors,&code,&flags);
      state(flags?CameraState::Certificate:CameraState::Offline);Serial.printf("Printer camera: TLS failed code=%d flags=%d.\n",code,flags);return false;
    }
    uint8_t auth[80];cameraAuth(auth,settings.accessCode);uint32_t authStarted=millis();
    bool ok=cameraReadExact([&](uint8_t *p,size_t n){return transfer(p,n,true,authStarted);},auth,sizeof(auth));memset(auth,0,sizeof(auth));
    if(!ok){if(!visible.load())cancelled();else state(CameraState::Offline);return false;}
    reader.reset(incoming,CameraJpegLimit);frameStarted=millis();schedule=CameraSchedule();pendingImage=false;
    xSemaphoreTake(mutex,portMAX_DELAY);streamOpen=true;++connections;xSemaphoreGive(mutex);
    Serial.printf("Printer camera: persistent connection ready in %u ms.\n",unsigned(millis()-started));return true;
  }
  bool publishImage(){
    uint32_t started=millis();size_t length=imageLength;
    int nativeWidth=0,nativeHeight=0;
    if(!cameraDimensions(jpeg,length,nativeWidth,nativeHeight)){state(CameraState::InvalidFrame);Serial.println("Printer camera: unsupported or malformed JPEG.");return false;}
    esp_jpeg_image_cfg_t decode={};decode.indata=jpeg;decode.indata_size=length;decode.out_format=JPEG_IMAGE_FORMAT_RGB565;decode.out_scale=JPEG_IMAGE_SCALE_1_4;
    decode.advanced.working_buffer=scratch;decode.advanced.working_buffer_size=65536;
    esp_jpeg_image_output_t info={};
    decode.outbuf=(uint8_t*)decoded;decode.outbuf_size=DecodedLimit;
    if(esp_jpeg_decode(&decode,&info)!=ESP_OK||info.width!=nativeWidth/4||info.height!=nativeHeight/4||info.output_len>DecodedLimit){state(CameraState::InvalidFrame);Serial.println("Printer camera: JPEG decode failed.");return false;}
    int width=0,height=0;if(!cameraFit(info.width,info.height,width,height)){state(CameraState::InvalidFrame);return false;}
    cameraResize(decoded,info.width,info.height,back,width,height);
    xSemaphoreTake(mutex,portMAX_DELAY);std::swap(front,back);view.pixels=front;view.width=width;view.height=height;view.received=imageReceived;intervalMs=view.revision?unsigned(millis()-lastPublished):0;lastPublished=millis();++view.revision;view.state=CameraState::Current;jpegBytes=length;decodeMs=millis()-started;xSemaphoreGive(mutex);
    Serial.printf("Printer camera: snapshot %u, %u bytes, decode=%u ms, interval=%u ms, image_age=%u ms.\n",view.revision,unsigned(length),decodeMs,intervalMs,unsigned(millis()-imageReceived));return true;
  }
  bool readStream(){
    if(uint32_t(millis()-frameStarted)>=CameraFrameTimeoutMs){state(CameraState::Offline);Serial.println("Printer camera: frame timeout; reconnecting.");return false;}
    size_t amount=std::min(reader.remaining(),size_t(4096));
    int count=int(esp_tls_conn_read(tls,reader.target(),amount));
    if(count==ESP_TLS_ERR_SSL_WANT_READ||count==ESP_TLS_ERR_SSL_WANT_WRITE){vTaskDelay(pdMS_TO_TICKS(10));return true;}
    if(count<=0){state(CameraState::Offline);Serial.println("Printer camera: stream ended; reconnecting.");return false;}
    auto result=reader.advance(size_t(count));
    if(result==CameraFrameReader::Result::Invalid){state(CameraState::InvalidFrame);Serial.println("Printer camera: invalid frame header.");return false;}
    if(result==CameraFrameReader::Result::Complete){
      int width=0,height=0;size_t length=reader.size();
      if(!cameraDimensions(incoming,length,width,height)){state(CameraState::InvalidFrame);Serial.println("Printer camera: unsupported or malformed JPEG.");return false;}
      // Keep only the newest complete JPEG. Continuous reads prevent queued old images.
      std::swap(jpeg,incoming);imageLength=length;imageReceived=millis();pendingImage=true;
      reader.reset(incoming,CameraJpegLimit);frameStarted=millis();retry.recovered();
      xSemaphoreTake(mutex,portMAX_DELAY);++receivedFrames;xSemaphoreGive(mutex);
    }
    vTaskDelay(1);return true;
  }
  static void task(void *arg){static_cast<Camera*>(arg)->run();}
  void run(){
    for(;;){
      if(!visible.load()){if(tls){close();cancelled();}retry=CameraRetry();vTaskDelay(pdMS_TO_TICKS(100));continue;}
      if(WiFi.status()!=WL_CONNECTED){close();retry=CameraRetry();state(CameraState::Offline);vTaskDelay(pdMS_TO_TICKS(250));continue;}
      if(time(nullptr)<1700000000){close();state(CameraState::Clock);vTaskDelay(pdMS_TO_TICKS(250));continue;}
      if(!tls){
        if(!retry.due(millis())){vTaskDelay(pdMS_TO_TICKS(100));continue;}
        if(!allocate()){retry.failed(millis());continue;}
        if(xSemaphoreTake(services->networkMutex,pdMS_TO_TICKS(100))!=pdTRUE)continue;
        bool ok=wanted()&&connect();
        // Only the handshake/authentication holds the shared network lock.
        // MQTT and public feeds can update while this camera stream stays open.
        xSemaphoreGive(services->networkMutex);
        if(!ok){close();retry.failed(millis());continue;}
      }
      if(!readStream()){close();retry.failed(millis());continue;}
      if(!wanted())continue;
      if(pendingImage&&schedule.due(true,millis())){
        schedule.started(millis());pendingImage=false;
        if(!publishImage()){close();retry.failed(millis());}
      }
    }
  }
};
}
