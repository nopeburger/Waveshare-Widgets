#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>

namespace spacewx {
constexpr int HistoryBins=73;
constexpr int64_t HistorySeconds=6*3600,FreshSeconds=15*60,WindFreshSeconds=20*60;
constexpr uint32_t RefreshMs=2*60*1000;
enum class Link:uint8_t {Waiting,Live,Offline,Clock,Error};
struct Outlook {int g=-1;int64_t day=0;};
struct Scales {int g=-1,s=-1,r=-1;int64_t stamp=0;Outlook outlook[3];};
struct Sample {int64_t stamp=0;float value=NAN;};
struct Kp {float value=NAN;int64_t stamp=0;Sample samples[HistoryBins];int count=0;};
struct Wind {float mph=NAN;int64_t stamp=0;};
struct Snapshot {
  Scales scales;Kp kp;Wind wind;
  Link links[3]={Link::Waiting,Link::Waiting,Link::Waiting};uint32_t revision=0;
};
inline bool timely(int64_t stamp,int64_t epoch,int64_t age=FreshSeconds){return stamp>0&&epoch>0&&stamp<=epoch+120&&epoch-stamp<=age;}
inline bool scalesFresh(const Snapshot &v,int64_t epoch){return v.links[0]==Link::Live&&timely(v.scales.stamp,epoch);}
inline bool kpFresh(const Snapshot &v,int64_t epoch){return v.links[1]==Link::Live&&isfinite(v.kp.value)&&timely(v.kp.stamp,epoch);}
inline bool windFresh(const Snapshot &v,int64_t epoch){return v.links[2]==Link::Live&&isfinite(v.wind.mph)&&timely(v.wind.stamp,epoch,WindFreshSeconds);}
inline int severity(const Scales &v){return std::max(v.g,std::max(v.s,v.r));}
inline bool complete(const Scales &v){return v.g>=0&&v.s>=0&&v.r>=0;}
inline const char *levelName(int level){static const char *names[]={"NONE","MINOR","MODERATE","STRONG","SEVERE","EXTREME"};return level>=0&&level<=5?names[level]:"NO DATA";}
inline int activity(const Snapshot &v,int64_t epoch){
  int result=scalesFresh(v,epoch)?std::max(0,severity(v.scales)):0;
  if(kpFresh(v,epoch)&&v.kp.value>=4)result=std::max(result,v.kp.value>=7?3:v.kp.value>=5?2:1);
  return result;
}
inline const char *headline(const Snapshot &v,int64_t epoch){
  if(scalesFresh(v,epoch)&&severity(v.scales)>0){
    int n=severity(v.scales);return n==5?"EXTREME ACTIVITY":n>=4?"SEVERE ACTIVITY":n>=3?"STRONG ACTIVITY":n>=2?"MODERATE ACTIVITY":"MINOR ACTIVITY";
  }
  if(kpFresh(v,epoch)&&v.kp.value>=4)return v.kp.value>=5?"HIGH KP":"ELEVATED KP";
  if(scalesFresh(v,epoch)&&complete(v.scales)&&kpFresh(v,epoch))return "QUIET";
  if(scalesFresh(v,epoch)||kpFresh(v,epoch))return "PARTIAL DATA";
  if(v.links[0]==Link::Clock||v.links[1]==Link::Clock)return "SYNCING TIME";
  if(v.links[0]==Link::Offline&&v.links[1]==Link::Offline)return "WI-FI OFFLINE";
  return v.scales.stamp||v.kp.stamp?"DATA DELAYED":"CONNECTING";
}
inline const char *summary(const Snapshot &v,int64_t epoch){
  if(scalesFresh(v,epoch)&&severity(v.scales)>0){
    int count=(v.scales.g>0)+(v.scales.s>0)+(v.scales.r>0);
    if(count>1)return "MULTIPLE NOAA SCALES";
    return v.scales.g>0?"GEOMAGNETIC STORM":v.scales.s>0?"SOLAR RADIATION STORM":"RADIO BLACKOUT";
  }
  if(kpFresh(v,epoch)&&v.kp.value>=4)return "MAGNETIC FIELD DISTURBED";
  if(scalesFresh(v,epoch)&&complete(v.scales)&&kpFresh(v,epoch))return "NO NOAA STORMS REPORTED";
  return "WAITING FOR CURRENT DATA";
}
inline void ageText(int64_t stamp,int64_t epoch,char *out,size_t size){
  if(!stamp||epoch<stamp-120){snprintf(out,size,"NO DATA");return;}
  int64_t age=std::max(int64_t(0),epoch-stamp);
  if(age<60)snprintf(out,size,"JUST NOW");else if(age<3600)snprintf(out,size,"%ld MIN AGO",long(age/60));
  else if(age<86400)snprintf(out,size,"%ld HR AGO",long(age/3600));else snprintf(out,size,"%ld DAYS AGO",long(age/86400));
}
inline void speedText(float mph,char *out,size_t size){if(!isfinite(mph))snprintf(out,size,"--");else if(mph>=1000000)snprintf(out,size,"%.2fM",double(mph/1000000));else snprintf(out,size,"%.0fK",double(mph/1000));}
}
