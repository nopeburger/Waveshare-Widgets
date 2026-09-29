#pragma once
#include "../../core/location.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

namespace quake {
constexpr int MaxEvents=32,LookbackDays=7;
constexpr float RadiusMiles=100;
inline double milesFromKm(double kilometers){return kilometers/1.609344;}
constexpr int64_t WindowSeconds=int64_t(LookbackDays)*86400,FreshSeconds=15*60;
constexpr uint32_t SlideMs=10000,RefreshMs=5*60*1000;
enum class Status {Waiting,Live,Offline,Clock,Error};
struct Event {
  char id[32]={},place[112]={};
  int64_t occurred=0,updated=0;
  float magnitude=NAN,depthKm=NAN,distance=0,bearing=0;
  bool reviewed=false;
};
struct Snapshot {
  Event events[MaxEvents];int count=0;
  double lat=NAN,lon=NAN;int64_t generated=0;
  uint32_t revision=0;Status status=Status::Waiting;
  char message[48]="CONNECTING TO USGS";
};
inline bool sameCenter(const Snapshot &s,const sky::Home &h){return isfinite(s.lat)&&isfinite(s.lon)&&fabs(s.lat-h.lat)<1e-7&&fabs(s.lon-h.lon)<1e-7;}
inline bool inWindow(const Event &e,int64_t epoch){return e.occurred>0&&e.occurred<=epoch+120&&epoch-e.occurred<=WindowSeconds;}
inline bool fresh(const Snapshot &s,int64_t epoch){return s.status==Status::Live&&s.generated>0&&s.generated<=epoch+120&&epoch-s.generated<=FreshSeconds;}
inline void ageLabel(int64_t occurred,int64_t epoch,char *out,size_t size){
  int64_t age=std::max(int64_t(0),epoch-occurred);
  if(age<60)snprintf(out,size,"JUST NOW");
  else if(age<3600)snprintf(out,size,"%ld MIN AGO",long(age/60));
  else if(age<86400)snprintf(out,size,"%ld HR AGO",long(age/3600));
  else snprintf(out,size,"%ld %s AGO",long(age/86400),age<172800?"DAY":"DAYS");
}
inline void dateLabel(int64_t epoch,char *out,size_t size){
  time_t stamp=time_t(epoch);tm local={};
#ifdef _WIN32
  localtime_s(&local,&stamp);
#else
  localtime_r(&stamp,&local);
#endif
  static const char *months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};
  snprintf(out,size,"%s %d / %d:%02d %s",months[std::max(0,std::min(local.tm_mon,11))],local.tm_mday,local.tm_hour%12?local.tm_hour%12:12,local.tm_min,local.tm_hour<12?"AM":"PM");
}
class Slideshow {
 public:
  Event events[MaxEvents];int count=0,index=0;bool held=false;uint32_t switched=0;
  void update(const Snapshot &snapshot,uint32_t now,int64_t epoch){
    char selected[32]={};if(current())snprintf(selected,sizeof(selected),"%s",current()->id);
    int total=0,found=-1;
    for(int i=0;i<std::min(snapshot.count,MaxEvents);i++)if(inWindow(snapshot.events[i],epoch)){
      events[total]=snapshot.events[i];if(!strcmp(events[total].id,selected))found=total;++total;
    }
    count=total;
    if(found>=0)index=found;else{index=0;held=false;switched=now;}
    if(count>1&&!held&&uint32_t(now-switched)>=SlideMs)advance(1,now);
  }
  void advance(int direction,uint32_t now){if(count)index=(index+direction+count)%count;switched=now;}
  void toggleHold(uint32_t now){if(count){held=!held;switched=now;}}
  const Event *current()const{return count&&index>=0&&index<count?&events[index]:nullptr;}
  float progress(uint32_t now)const{return held?1.f:std::min(1.f,uint32_t(now-switched)/float(SlideMs));}
};
}
