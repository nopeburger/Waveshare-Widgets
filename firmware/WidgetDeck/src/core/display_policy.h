#pragma once
#include <stdint.h>

namespace deck {
// Shared by every widget and the Wi-Fi setup screen. Local minutes come from
// the configured timezone; elapsed touch time uses the monotonic clock.
struct DisplayPolicy {
  struct Settings {
    bool nightEnabled=false;
    uint8_t dayPercent=85,nightPercent=20,touchPercent=100;
    int nightStart=22*60+30,nightEnd=7*60;
    uint32_t touchTimeout=30000;
  } settings;
  bool clockKnown=false,night=false,awake=false;
  uint8_t percent=85;
  uint32_t touchedAt=0;

  bool isNight(int minute)const{
    if(settings.nightStart<settings.nightEnd)return minute>=settings.nightStart&&minute<settings.nightEnd;
    return minute>=settings.nightStart||minute<settings.nightEnd;
  }
  static uint8_t panelValue(uint8_t percent){return (uint16_t(percent)*255+50)/100;}
  void touch(uint32_t now){touchedAt=now;awake=true;}
  void clearTouch(){awake=false;}
  void tick(uint32_t now,int localMinute){
    // Retire expired touches so they cannot become recent again at millis wrap.
    if(awake&&uint32_t(now-touchedAt)>=settings.touchTimeout)awake=false;
    clockKnown=localMinute>=0&&localMinute<24*60;
    // An unknown clock uses the daytime level. Night mode is opt-in.
    night=settings.nightEnabled&&clockKnown&&isNight(localMinute);
    percent=!night?settings.dayPercent:awake?settings.touchPercent:settings.nightPercent;
  }
};
}
