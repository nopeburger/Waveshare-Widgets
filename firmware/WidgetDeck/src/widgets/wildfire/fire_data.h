#pragma once
#include "../../core/location.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

namespace wildfire {
constexpr int MaxReports=16,PageSize=128,MaxPages=16;
constexpr float RadiusMiles=100;
constexpr uint32_t SlideMs=10000,RefreshMs=5*60*1000;
constexpr int64_t FreshSeconds=15*60,CacheSeconds=24*60*60;
enum class Status {Waiting,Current,Offline,Clock,Error};
struct Report {
  char id[40]={},name[64]={},county[64]={};
  int64_t discovered=0,updated=0;
  float acres=NAN,contained=NAN,distance=0,bearing=0;
};
struct Snapshot {
  Report reports[MaxReports];int count=0;
  double lat=NAN,lon=NAN;int64_t received=0;
  uint32_t revision=0;Status status=Status::Waiting;
};
inline bool sameCenter(const Snapshot &s,const sky::Home &h){return isfinite(s.lat)&&isfinite(s.lon)&&fabs(s.lat-h.lat)<1e-7&&fabs(s.lon-h.lon)<1e-7;}
inline bool usable(const Snapshot &s,int64_t epoch){return s.received>0&&s.received<=epoch+120&&epoch-s.received<=CacheSeconds;}
inline bool fresh(const Snapshot &s,int64_t epoch){return s.status==Status::Current&&usable(s,epoch)&&epoch-s.received<=FreshSeconds;}
inline void updatedLabel(int64_t stamp,int64_t epoch,char *out,size_t size){
  if(stamp<=0){snprintf(out,size,"UPDATE UNKNOWN");return;}
  int64_t age=std::max(int64_t(0),epoch-stamp);
  if(age<60)snprintf(out,size,"UPDATED JUST NOW");
  else if(age<3600)snprintf(out,size,"UPDATED %ld MIN AGO",long(age/60));
  else if(age<86400)snprintf(out,size,"UPDATED %ld HR AGO",long(age/3600));
  else snprintf(out,size,"UPDATED %ld %s AGO",long(age/86400),age<172800?"DAY":"DAYS");
}
inline void acresLabel(float acres,char *out,size_t size){
  if(!isfinite(acres)){snprintf(out,size,"--");return;}
  if(acres>0&&acres<.1f){snprintf(out,size,"<0.1");return;}
  if(acres<100){snprintf(out,size,"%.1f",double(acres));return;}
  if(acres>=1000000){snprintf(out,size,"%.1fM",double(acres/1000000));return;}
  unsigned value=unsigned(lroundf(acres));
  if(value>=1000)snprintf(out,size,"%u,%03u",value/1000,value%1000);else snprintf(out,size,"%u",value);
}
class Slideshow {
 public:
  Report reports[MaxReports];int count=0,index=0;bool held=false;uint32_t switched=0;
  const Report *current()const{return count&&index>=0&&index<count?&reports[index]:nullptr;}
  void update(const Snapshot &snapshot,uint32_t now,int64_t epoch){
    char selected[40]={};if(current())snprintf(selected,sizeof(selected),"%s",current()->id);
    count=usable(snapshot,epoch)?std::min(snapshot.count,MaxReports):0;int found=-1;
    for(int i=0;i<count;i++){reports[i]=snapshot.reports[i];if(!strcmp(reports[i].id,selected))found=i;}
    if(found>=0)index=found;else{index=0;held=false;switched=now;}
    if(count>1&&!held&&uint32_t(now-switched)>=SlideMs)advance(1,now);
  }
  void advance(int direction,uint32_t now){if(count)index=(index+direction+count)%count;switched=now;}
  void toggleHold(uint32_t now){if(count){held=!held;switched=now;}}
  float progress(uint32_t now)const{return held?1.f:std::min(1.f,uint32_t(now-switched)/float(SlideMs));}
};
}
