#pragma once
#include <Arduino.h>
#include <atomic>
#include <new>
#include "../../core/services.h"
#include "sky_feed.h"
#include "sky_draw.h"
#include "sky_trust.h"
#include "sky_local_feed.h"
#include "sky_metadata.h"
#include "sky_meshpoint.h"
#include "sky_trail_draw.h"
namespace sky {
class Runtime {
 public:
  Home home;Slideshow show;Snapshot view;bool demo=false;
  void begin(deck::Services &shared){
    services=&shared;mutex=xSemaphoreCreateMutex();assert(mutex);loadSource();
    void *memory=heap_caps_malloc(sizeof(Metadata),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if(memory)metadata=new(memory)Metadata();
    memory=heap_caps_malloc(sizeof(Trails),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(memory)trails=new(memory)Trails();
    if(xTaskCreatePinnedToCore(networkTask,"flight-public",12288,this,1,&publicWorker,0)!=pdPASS){Serial.println("ADSB: public feed task failed.");if(configured.source==Source::Public)publishError(Status::Error,"NETWORK TASK FAILED",sourceRevision);}
    if(xTaskCreatePinnedToCore(localTask,"flight-local",12288,this,1,&localWorker,0)!=pdPASS){Serial.println("ADSB: local feed task failed.");if(configured.source==Source::MeshPoint)publishError(Status::Error,"NETWORK TASK FAILED",sourceRevision);}
  }
  void tick(uint32_t now,bool active){
    uint32_t revision;home=services->location(revision);
    if(staging&&uint32_t(now-stagingAt)>120000)clearStaging();
    if(demo)view=demoSnapshot(now);else{xSemaphoreTake(mutex,portMAX_DELAY);view=latest;xSemaphoreGive(mutex);}
    if(!demo&&view.locationRevision!=revision){bool local=view.localSource;view=Snapshot();view.localSource=local;}
    if(trails){
      if(revision!=trailLocation){trails->reset();trails->configureAirports(home);trailLocation=revision;trailRevision=UINT32_MAX;}
      if(demo||view.revision!=trailRevision){trails->update(view,now);trailRevision=view.revision;}
      trails->prune(now);
      bool airportExit=trails->excludeAirportTracking(now);
      if(airportExit||trails->expireAutomatic(now)){
        show.held=false;show.switched=now;
        Serial.println(airportExit?"ADSB tracking: aircraft entered airport buffer; resuming slideshow.":"ADSB tracking: aircraft unseen for 60 seconds; resuming slideshow.");
      }
    }
    if(!active||(trails&&trails->tracking()))show.switched=now;show.update(view,now);
    if(trails&&active&&!demo&&!services->setupVisible&&view.status==Status::Live&&uint32_t(now-lastDetection)>=2000){
      lastDetection=now;
      if(trails->autoTrack(now)){
        for(int i=0;i<show.count;i++)if(!strcmp(show.aircraft[i].hex,trails->selectedHex()))show.index=i;
        show.switched=now;Serial.printf("ADSB tracking: circling detected for %s.\n",trails->selectedHex());
      }
    }
  }
  void toggleTracking(uint32_t now){
    if(!trails)return;
    if(trails->tracking())trails->leave(now);
    else if(show.current())trails->select(show.current()->hex);
    show.held=false;show.switched=now;
  }
  void advance(int direction,uint32_t now){
    if(trails&&trails->tracking()&&show.count){
      int found=-1;for(int i=0;i<show.count;i++)if(!strcmp(show.aircraft[i].hex,trails->selectedHex()))found=i;
      show.index=found>=0?found:direction>0?-1:0;show.advance(direction,now);trails->select(show.current()->hex);
    }else show.advance(direction,now);
  }
  bool command(const char *line){
    if(!strncmp(line,"sky config ",11))return configCommand(line+11);
    if(!strcmp(line,"sky demo")){demo=true;show=Slideshow();if(trails)trails->reset();trailRevision=UINT32_MAX;return true;}
    if(!strcmp(line,"sky live")){demo=false;show=Slideshow();if(trails)trails->reset();trailRevision=UINT32_MAX;return true;}
    if(!strcmp(line,"sky status")){
      int branded=0,special=0,identified=0;for(int i=0;i<show.count;i++){if(airline(show.aircraft[i]))++branded;if(role(show.aircraft[i])!=Role::Standard)++special;if(show.aircraft[i].type[0])++identified;}
      const Aircraft *selected=show.current();const Airline *brand=selected?airline(*selected):nullptr;
      uint32_t revision;SourceSettings settings=sourceSettings(revision);
      Serial.printf("flight count=%d state=%d hold=%d demo=%d branded=%d services=%d identified=%d selected=%s role=%s airline=%s source=%s samples=%u heartbeat=%u age_ms=%u local_free=%u public_free=%u\n",show.count,int(view.status),show.held,demo,branded,special,identified,selected?callsign(*selected):"--",selected&&role(*selected)!=Role::Standard?roleName(role(*selected)):"STANDARD",brand?brand->code:"--",settings.source==Source::MeshPoint?"meshpoint":"public",samples.load(),heartbeats.load(),view.received?uint32_t(millis()-view.received):0,localWorker?unsigned(uxTaskGetStackHighWaterMark(localWorker)):0,publicWorker?unsigned(uxTaskGetStackHighWaterMark(publicWorker)):0);
      const Trail *track=trails?trails->current():nullptr;
      Serial.printf("tracking=%s automatic=%d points=%u seen_ms=%u history_bytes=%u radius=%.0fmi airport_buffer=%.0fmi airports=%u\n",track?track->aircraft.hex:"off",trails&&trails->automatic(),track?track->count:0,track?uint32_t(millis()-track->lastSeen):0,unsigned(sizeof(Trails)),double(home.radius),double(AirportBufferMiles),trails?trails->airportCount():0);return true;
    }
    return false;
  }
  void render(Canvas &canvas,uint32_t now){
    if(trails&&trails->current()){drawTrail(canvas,*trails->current(),view,now,demo,trails->automatic());return;}
    if(show.count){
      bool changed=strcmp(lastHex,show.current()->hex)!=0;
      if(changed){snprintf(lastHex,sizeof(lastHex),"%s",show.current()->hex);transitionAt=now;}
      float t=std::min(1.f,uint32_t(now-transitionAt)/280.f);
      canvas.card(show,view,home,now,demo,int((1-t)*(1-t)*24));return;
    }
    if(view.status==Status::Live)canvas.message("QUIET SKIES","NO AIRCRAFT NEARBY","WE'LL KEEP WATCHING",home,view.status,view.localSource);
    else if(view.status==Status::Clock)canvas.message("SYNCING TIME","SETTING THE CLOCK","SECURE CONNECTION",home,view.status,view.localSource);
    else if(view.status==Status::Offline)canvas.message("WI-FI OFFLINE","RECONNECTING TO WI-FI","HOLD FOR SETTINGS",home,view.status,view.localSource);
    else if(view.status==Status::Error)canvas.message("FEED UNAVAILABLE",view.message,"TRYING AGAIN SHORTLY",home,view.status,view.localSource);
    else if(view.status==Status::Setup)canvas.message("RECEIVER SETUP","CHECK ADSB.TXT","SAVE CONFIG AND RETRY",home,view.status,view.localSource);
    else canvas.message("LOOKING UP",view.localSource?"CONNECTING TO YOUR PI":"CONNECTING TO ADSB.LOL","SEARCHING YOUR AREA",home,view.status,view.localSource);
  }
  static Snapshot demoSnapshot(uint32_t now) {
    Snapshot s;s.status=Status::Live;s.received=now;s.count=3;
    auto &a=s.aircraft[0];strcpy(a.hex,"DEMO01");strcpy(a.flight,"SKY240");strcpy(a.registration,"N240SK");strcpy(a.type,"B738");a.altitude=12400;a.speed=286;a.distance=2.4;a.bearing=45;a.track=312;a.verticalRate=-768;
    auto &b=s.aircraft[1];strcpy(b.hex,"DEMO02");strcpy(b.flight,"N172SK");strcpy(b.registration,"N172SK");strcpy(b.type,"C172");strcpy(b.category,"A1");b.altitude=3500;b.speed=112;b.distance=4.8;b.bearing=230;b.track=82;b.verticalRate=0;
    auto &c=s.aircraft[2];strcpy(c.hex,"DEMO03");strcpy(c.flight,"SKY HELI");strcpy(c.type,"R44");strcpy(c.category,"A7");c.altitude=1250;c.speed=90;c.distance=6.1;c.bearing=140;c.track=154;c.verticalRate=320;
    return s;
  }
 private:
  deck::Services *services=nullptr;SemaphoreHandle_t mutex=nullptr;Snapshot latest;
  SourceSettings configured;uint32_t sourceRevision=1;Metadata *metadata=nullptr;
  Trails *trails=nullptr;uint32_t trailRevision=UINT32_MAX,trailLocation=UINT32_MAX,lastDetection=0;
  TaskHandle_t publicWorker=nullptr,localWorker=nullptr;
  std::atomic<uint32_t> samples{0},heartbeats{0};
  char lastHex[9]={};uint32_t transitionAt=0;
  char *staging=nullptr;size_t stagingSize=0;uint32_t stagingAt=0;
  SourceSettings sourceSettings(uint32_t &revision){xSemaphoreTake(mutex,portMAX_DELAY);SourceSettings s=configured;revision=sourceRevision;xSemaphoreGive(mutex);return s;}
  void clearStaging(){if(staging){memset(staging,0,2049);free(staging);staging=nullptr;}stagingSize=0;}
  bool configCommand(const char *line){
    if(!strcmp(line,"begin")){
      clearStaging();staging=(char*)heap_caps_calloc(2049,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);stagingAt=millis();
      Serial.println(staging?"ADSB config ready (credentials hidden).":"ADSB config failed: allocation.");return true;
    }
    if(!strcmp(line,"abort")){clearStaging();Serial.println("ADSB config cancelled.");return true;}
    if(!strncmp(line,"line ",5)){
      size_t size=strlen(line+5);
      if(!staging||size+1>2048-stagingSize){clearStaging();Serial.println("ADSB config failed: size or sequence.");return true;}
      memcpy(staging+stagingSize,line+5,size);stagingSize+=size;staging[stagingSize++]='\n';staging[stagingSize]=0;stagingAt=millis();
      Serial.println("ADSB config line accepted.");return true;
    }
    if(!strcmp(line,"commit")){
      SourceSettings next;
      if(!staging||!parseSourceSettings(staging,stagingSize,next)){clearStaging();Serial.println("ADSB config failed: invalid settings.");return true;}
      Preferences prefs;bool saved=prefs.begin("deck-adsb",false)&&prefs.putString("settings",staging)>0;prefs.end();
      if(saved){
        xSemaphoreTake(mutex,portMAX_DELAY);configured=next;++sourceRevision;latest=Snapshot();latest.localSource=next.source==Source::MeshPoint;xSemaphoreGive(mutex);
        demo=false;show=Slideshow();if(trails)trails->reset();trailRevision=UINT32_MAX;Serial.println("ADSB config saved (credentials hidden).");
      }else Serial.println("ADSB config failed: storage.");
      memset(&next,0,sizeof(next));clearStaging();return true;
    }
    Serial.println("ADSB config failed: unknown command.");return true;
  }
  void loadSource(){
    Preferences prefs;if(!prefs.begin("deck-adsb",false))return;
    String saved=prefs.getString("settings","");
    if(saved.length()&&!parseSourceSettings(saved.c_str(),saved.length(),configured))configured.source=Source::MeshPoint;
    char *raw=(char*)heap_caps_calloc(2049,1,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);size_t size=0;int state=0;
    if(raw&&readCardFile("adsb.txt",raw,2049,size,state)){
      SourceSettings card;uint64_t hash=sourceFingerprint(raw,size);
      if(parseSourceSettings(raw,size,card)){
        if(!saved.length()||!prefs.isKey("sd-hash")||prefs.getULong64("sd-hash",0)!=hash){
          if(prefs.putString("settings",raw)>0){configured=card;prefs.putULong64("sd-hash",hash);Serial.println("ADSB: applied SD configuration (credentials hidden).");}
        }
      }else Serial.println("ADSB: invalid adsb.txt; retaining saved configuration.");
      memset(&card,0,sizeof(card));
    }
    if(raw){memset(raw,0,2049);free(raw);}prefs.end();latest.localSource=configured.source==Source::MeshPoint;
    if(!validSourceSettings(configured))latest.status=Status::Setup;
  }
  void publishError(Status status,const char *message,uint32_t expected){
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(sourceRevision==expected){latest.status=status;snprintf(latest.message,sizeof(latest.message),"%s",message);}
    xSemaphoreGive(mutex);
  }
  bool publish(Snapshot &parsed,uint32_t expected,uint32_t locationRevision,bool local){
    uint32_t currentLocation;services->location(currentLocation);bool accepted=false;
    xSemaphoreTake(mutex,portMAX_DELAY);
    if(sourceRevision==expected&&locationRevision==currentLocation){
      parsed.locationRevision=locationRevision;
      if(local){if(metadata)metadata->enrich(parsed,millis());parsed.revision=latest.revision+1;latest=parsed;}
      else{
        if(metadata)metadata->update(parsed,millis());
        if(configured.source==Source::Public){parsed.revision=latest.revision+1;latest=parsed;}
      }
      accepted=true;
    }
    xSemaphoreGive(mutex);return accepted;
  }
  void clearLocation(uint32_t expected){
    xSemaphoreTake(mutex,portMAX_DELAY);if(sourceRevision==expected){bool local=latest.localSource;latest=Snapshot();latest.localSource=local;}xSemaphoreGive(mutex);
  }
  static void localTask(void *context){static_cast<Runtime*>(context)->localLoop();}
  void localLoop(){
    uint32_t lastConfig=0,lastLocation=0,due=0,backoff=2000;MeshPoint *session=nullptr;
    for(;;){
      uint32_t configRevision,locationRevision;SourceSettings settings=sourceSettings(configRevision);Home h=services->location(locationRevision);
      if(configRevision!=lastConfig){
        if(session){session->~MeshPoint();memset(session,0,sizeof(MeshPoint));free(session);session=nullptr;}
        lastConfig=configRevision;due=0;backoff=2000;heartbeats=0;samples=0;
        if(settings.source==Source::MeshPoint){void *memory=heap_caps_malloc(sizeof(MeshPoint),MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);if(memory)session=new(memory)MeshPoint();}
      }
      if(settings.source!=Source::MeshPoint){vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(locationRevision!=lastLocation){lastLocation=locationRevision;clearLocation(configRevision);due=0;}
      if(!validSourceSettings(settings)||!session){publishError(Status::Setup,"CHECK ADSB.TXT",configRevision);vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(WiFi.status()!=WL_CONNECTED){publishError(Status::Offline,"WI-FI OFFLINE",configRevision);vTaskDelay(pdMS_TO_TICKS(500));continue;}
      session->tick(settings);heartbeats=session->heartbeats;
      if(!session->healthy(millis())){publishError(Status::Error,session->error,configRevision);vTaskDelay(pdMS_TO_TICKS(200));continue;}
      uint32_t now=millis();if(due&&int32_t(now-due)<0){vTaskDelay(pdMS_TO_TICKS(100));continue;}
      uint32_t started=now;char url[96];snprintf(url,sizeof(url),"http://%s:%u/data.json",settings.ip,settings.receiverPort);
      Body body;int code=localRequest(url,body);Snapshot parsed;
      bool success=code==200&&parseLocalFeed(body.data,body.length,h,millis(),parsed)&&publish(parsed,configRevision,locationRevision,true);
      if(success){
        ++samples;backoff=2000;due=started+2000;
        if(samples.load()%15==1)Serial.printf("ADSB local: %d nearby, %u bytes, sample=%u, fetch=%ums.\n",parsed.count,unsigned(body.length),samples.load(),uint32_t(millis()-started));
      }else{
        publishError(Status::Error,code==200?"INVALID RECEIVER DATA":"RECEIVER UNREACHABLE",configRevision);
        Serial.printf("ADSB local fetch failed (HTTP %d). Retaining recent aircraft.\n",code);
        due=millis()+std::max(backoff,body.retryAfter);backoff=std::min(backoff*2,uint32_t(15000));
      }
      vTaskDelay(pdMS_TO_TICKS(10));
    }
  }
  static void networkTask(void *context){static_cast<Runtime*>(context)->fetchLoop();}
  void fetchLoop(){
    uint32_t due=0,backoff=10000,lastRevision=0,lastSource=0;
    for(;;){
      uint32_t revision,configRevision;Home h=services->location(revision);SourceSettings settings=sourceSettings(configRevision);bool local=settings.source==Source::MeshPoint;
      if(revision!=lastRevision||configRevision!=lastSource){if(!local)clearLocation(configRevision);due=0;backoff=10000;lastRevision=revision;lastSource=configRevision;}
      uint32_t now=millis();
      if(WiFi.status()!=WL_CONNECTED){if(!local)publishError(Status::Offline,"WI-FI OFFLINE",configRevision);vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(time(nullptr)<1700000000){if(!local)publishError(Status::Clock,"WAITING FOR NETWORK TIME",configRevision);vTaskDelay(pdMS_TO_TICKS(500));continue;}
      if(due&&int32_t(now-due)<0){vTaskDelay(pdMS_TO_TICKS(200));continue;}
      xSemaphoreTake(services->networkMutex,portMAX_DELAY);
      char url[192];snprintf(url,sizeof(url),"https://api.adsb.lol/v2/point/%.7f/%.7f/%d",h.lat,h.lon,int(ceil(h.radius/1.150779448)));
      bool success=false;char error[48]="CONNECTION FAILED";uint32_t retryAfter=0;
      {
        Body body;
        esp_http_client_config_t config={};config.url=url;config.cert_pem=skyRootCa;config.timeout_ms=10000;
        config.event_handler=Body::receive;config.user_data=&body;config.disable_auto_redirect=true;config.buffer_size=4096;
        esp_http_client_handle_t client=body.data?esp_http_client_init(&config):nullptr;
        if(client){
          esp_http_client_set_header(client,"Accept","application/json");esp_http_client_set_header(client,"Accept-Encoding","identity");
          esp_http_client_set_header(client,"User-Agent","NearbySky-ESP32/1.0");
          esp_err_t result=esp_http_client_perform(client);int code=esp_http_client_get_status_code(client);
          bool complete=esp_http_client_is_complete_data_received(client);retryAfter=body.retryAfter;esp_http_client_cleanup(client);
          if(body.overflow)snprintf(error,sizeof(error),"RESPONSE TOO LARGE");
          else if(result!=ESP_OK)snprintf(error,sizeof(error),"NETWORK ERROR %d",int(result));
          else if(code!=200)snprintf(error,sizeof(error),"HTTP ERROR %d",code);
          else if(!complete)snprintf(error,sizeof(error),"INCOMPLETE RESPONSE");
          else{
            Snapshot parsed;
            if(parseFeed(body.data,body.length,h,double(time(nullptr)),millis(),parsed)){
              success=publish(parsed,configRevision,revision,false);
              if(success)Serial.printf("ADSB %s: %d nearby aircraft, %u bytes.\n",local?"metadata":"public",parsed.count,unsigned(body.length));
            }else snprintf(error,sizeof(error),"INVALID / OLD DATA");
          }
        }
      }
      xSemaphoreGive(services->networkMutex);
      if(success){backoff=10000;due=millis()+(local?60000:10000);}
      else{
        Serial.printf("ADSB %s fetch failed: %s.\n",local?"metadata":"public",error);
        if(!local)publishError(Status::Error,error,configRevision);
        backoff=std::min(backoff*2,uint32_t(120000));due=millis()+std::max(backoff,retryAfter);
      }
    }
  }
};
}
