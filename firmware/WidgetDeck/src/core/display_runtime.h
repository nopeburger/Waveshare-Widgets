#pragma once
#include <Arduino.h>
#include <time.h>
#include "board.h"
#include "display_policy.h"
#include "../../config.h"

namespace deck {
static_assert(DECK_NIGHT_MODE_ENABLED==0||DECK_NIGHT_MODE_ENABLED==1,"Night mode must be 0 or 1");
static_assert(DECK_DAY_BRIGHTNESS_PERCENT>=1&&DECK_DAY_BRIGHTNESS_PERCENT<=100,"Day brightness must be 1-100");
static_assert(DECK_NIGHT_BRIGHTNESS_PERCENT>=1&&DECK_NIGHT_BRIGHTNESS_PERCENT<=100,"Night brightness must be 1-100");
static_assert(DECK_TOUCH_BRIGHTNESS_PERCENT>=1&&DECK_TOUCH_BRIGHTNESS_PERCENT<=100,"Touch brightness must be 1-100");
static_assert(DECK_NIGHT_START_MINUTE>=0&&DECK_NIGHT_START_MINUTE<1440,"Night start must be 0-1439");
static_assert(DECK_NIGHT_END_MINUTE>=0&&DECK_NIGHT_END_MINUTE<1440,"Night end must be 0-1439");
static_assert(DECK_NIGHT_START_MINUTE!=DECK_NIGHT_END_MINUTE,"Night start and end must differ");
static_assert(DECK_NIGHT_TOUCH_TIMEOUT_MS>=1000,"Night touch timeout must be at least 1000 ms");
class Display {
 public:
  Display(){
    policy.settings={bool(DECK_NIGHT_MODE_ENABLED),DECK_DAY_BRIGHTNESS_PERCENT,DECK_NIGHT_BRIGHTNESS_PERCENT,
      DECK_TOUCH_BRIGHTNESS_PERCENT,DECK_NIGHT_START_MINUTE,DECK_NIGHT_END_MINUTE,DECK_NIGHT_TOUCH_TIMEOUT_MS};
    policy.tick(0,-1);
  }
  void begin(){board_brightness(DisplayPolicy::panelValue(policy.percent));applied=policy.percent;}
  void touch(uint32_t now){policy.touch(now);}
  void tick(uint32_t now){
    time_t epoch=time(nullptr);
    if(epoch!=lastEpoch){
      lastEpoch=epoch;localMinute=-1;
      struct tm local={};
      if(epoch>1700000000&&localtime_r(&epoch,&local))localMinute=local.tm_hour*60+local.tm_min;
    }
    if(testMinute>=0&&uint32_t(now-testStarted)>=TestDuration){
      testMinute=-1;Serial.println("Display test finished; automatic schedule restored.");
    }
    policy.tick(now,testMinute>=0?testMinute:localMinute);
    if(applied!=policy.percent){
      board_brightness(DisplayPolicy::panelValue(policy.percent));applied=policy.percent;
      Serial.printf("Display brightness: %u%% (%s).\n",unsigned(applied),mode());
    }
  }
  bool command(const char *line){
    if(!strcmp(line,"display status")){status();return true;}
    if(!strcmp(line,"display wake"))policy.touch(millis());
    else if(!strcmp(line,"display test night")||!strcmp(line,"display test day")){
      testMinute=!strcmp(line,"display test night")?policy.settings.nightStart:policy.settings.nightEnd;
      testStarted=millis();policy.clearTouch();
      Serial.println("Display schedule test: 120 seconds; system clock unchanged. Enable night mode to test dimming.");
    }else if(!strcmp(line,"display auto")){testMinute=-1;policy.clearTouch();}
    else return false;
    tick(millis());status();return true;
  }
  void status(){
    char local[8]="--:--";if(localMinute>=0)snprintf(local,sizeof(local),"%02d:%02d",localMinute/60,localMinute%60);
    Serial.printf("display=%u%% panel=%u mode=%s local=%s night=%s schedule=%02d:%02d-%02d:%02d day=%u%% dim=%u%% touch=%u%% touch_timeout=%ums test=%s leds=hardware-only\n",
      unsigned(policy.percent),unsigned(DisplayPolicy::panelValue(policy.percent)),mode(),
      local,policy.settings.nightEnabled?"on":"off",policy.settings.nightStart/60,policy.settings.nightStart%60,
      policy.settings.nightEnd/60,policy.settings.nightEnd%60,unsigned(policy.settings.dayPercent),
      unsigned(policy.settings.nightPercent),unsigned(policy.settings.touchPercent),
      unsigned(policy.settings.touchTimeout),testMinute>=0?"active":"off");
  }
 private:
  DisplayPolicy policy;uint8_t applied=0;time_t lastEpoch=0;int localMinute=-1,testMinute=-1;
  static constexpr uint32_t TestDuration=120000;
  uint32_t testStarted=0;
  const char *mode()const{return !policy.settings.nightEnabled?"fixed":!policy.clockKnown?"clock-wait":!policy.night?"day":policy.awake?"night-touch":"night-dim";}
};
}
