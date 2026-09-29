#include <Arduino.h>
#include <time.h>
#include <atomic>
#include "esp_heap_caps.h"
#include "config.h"
#include "src/core/board.h"
#include "src/core/display_runtime.h"
#include "src/core/services.h"
#include "src/widgets/registry.h"

static deck::Services services;
static deck::Touch touch;
static deck::Display display;
static std::atomic<bool> touchActivity{false};
static QueueHandle_t gestures;
static Preferences preferences;
static uint16_t *pixels;
static uint32_t lastFrame=0,changedAt=0,eventCount=0;
static std::atomic<uint32_t> touchSamples{0},touchErrors{0};
static uint32_t renderMs=0,presentMs=0;
static bool dirty=true,saveSelection=false;
static char command[640];static size_t commandLength=0;static bool commandOverflow=false;

static void touchTask(void *){
  for(;;){int x=0,y=0,contact=board_touch(&x,&y);if(contact<0)touchErrors++;else touchSamples++;
    // Wake on contact, including stationary holds, without waiting for a gesture.
    // The main loop alone writes panel commands, after any frame transfer ends.
    if(contact>0)touchActivity.store(true);
    deck::Event event=touch.update(contact,x,y,millis());if(event!=deck::Event::None)xQueueSend(gestures,&event,0);
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
static void dispatch(deck::Event event){
  uint32_t now=millis();eventCount++;dirty=true;
  if(event==deck::Event::Hold){services.toggleSetup();return;}
  if(services.setupVisible){
    if(event==deck::Event::Left||event==deck::Event::Right)services.setupVisible=false;
    else return;
  }
  if(deck::app.dispatch(event,now)){changedAt=now;saveSelection=true;Serial.printf("Widget: %s\n",deck::app.active().id());}
}
static void parseCommand(){
  if(!strcmp(command,"next"))dispatch(deck::Event::Left);
  else if(!strcmp(command,"previous"))dispatch(deck::Event::Right);
  else if(!strcmp(command,"up"))dispatch(deck::Event::Up);
  else if(!strcmp(command,"down"))dispatch(deck::Event::Down);
  else if(!strcmp(command,"tap"))dispatch(deck::Event::Tap);
  else if(!strcmp(command,"settings"))dispatch(deck::Event::Hold);
  else if(!strncmp(command,"widget ",7)){if(deck::app.select(command+7)){services.setupVisible=false;saveSelection=true;changedAt=millis();}}
  else if(!strcmp(command,"status")){
    Serial.printf("widget=%s widgets=%u render=%ums present=%ums touch=%u errors=%u gestures=%u heap=%u PSRAM=%u\n",deck::app.active().id(),deck::app.count,renderMs,presentMs,touchSamples.load(),touchErrors.load(),eventCount,ESP.getFreeHeap(),ESP.getFreePsram());
    display.status();
    services.command("wifi status");for(unsigned i=0;i<deck::app.count;i++){char line[40];snprintf(line,sizeof(line),"%s status",deck::app.widgets[i]->id());if(!strcmp(line,"flight status"))strcpy(line,"sky status");deck::app.widgets[i]->command(line);}
  }else if(!display.command(command)&&!services.command(command)){
    bool handled=false;for(unsigned i=0;i<deck::app.count;i++)handled=deck::app.widgets[i]->command(command)||handled;
    if(!handled)Serial.println("Commands: widget ID | next | previous | up | down | tap | settings | status | display status | display test night/day | display wake | display auto | wifi scan | sky live | sky demo | gas refresh | quake refresh | fire refresh | printer status | space refresh");
  }
  dirty=true;
}
void setup(){
  Serial.begin(115200);
  pixels=(uint16_t*)heap_caps_malloc(ui::Width*ui::Height*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!pixels){Serial.println("Enable OPI PSRAM for Widget Deck.");while(true)delay(1000);}
  board_init();display.begin();ui::Canvas canvas(pixels);canvas.clear();canvas.text(20,190,"WIDGET DECK",ui::fontNumber,ui::rgb(231,240,242));board_present_native(pixels);
  preferences.begin("widget-deck",false);String saved=preferences.getString("widget","flight");deck::app.select(saved.c_str());
  gestures=xQueueCreate(8,sizeof(deck::Event));assert(gestures);
  if(xTaskCreatePinnedToCore(touchTask,"deck-touch",3072,nullptr,3,nullptr,0)!=pdPASS){Serial.println("Touch task failed.");while(true)delay(1000);}
  configTzTime(DECK_TIMEZONE,"pool.ntp.org","time.nist.gov");services.begin(GAS_WIFI_SSID,GAS_WIFI_PASSWORD);
  for(unsigned i=0;i<deck::app.count;i++)deck::app.widgets[i]->begin(services);
  Serial.println("Widget Deck ready. Left/right: widgets. Up/down: aircraft, earthquakes, wildfires or printer/space pages. Tap: track aircraft / hold quake or fire / refresh gas or space / printer page. Hold: settings.");
}
void loop(){
  if(touchActivity.exchange(false))display.touch(millis());
  display.tick(millis());
  uint32_t now=millis();services.tick(now);deck::app.tick(now);
  deck::Event event;while(xQueueReceive(gestures,&event,0)==pdTRUE)dispatch(event);
  while(Serial.available()){
    char c=Serial.read();
    if(c=='\n'||c=='\r'){
      if(commandOverflow){for(unsigned i=0;i<deck::app.count;i++)deck::app.widgets[i]->command("sky config abort");Serial.println("Command too long; discarded.");}
      else if(commandLength){command[commandLength]=0;parseCommand();}
      memset(command,0,sizeof(command));commandLength=0;commandOverflow=false;
    }else if(!commandOverflow){if(commandLength<sizeof(command)-1)command[commandLength++]=c;else commandOverflow=true;}
  }
  now=millis();
  if(saveSelection&&uint32_t(now-changedAt)>2000){preferences.putString("widget",deck::app.active().id());saveSelection=false;}
  if(dirty||uint32_t(now-lastFrame)>=(services.setupVisible?200:deck::app.active().frameMs())){
    lastFrame=now;dirty=false;uint32_t started=millis();
    if(services.setupVisible)services.renderSetup(pixels);else deck::app.render(pixels,now);
    renderMs=millis()-started;started=millis();board_present_native(pixels);presentMs=millis()-started;
  }
  delay(1);
}
