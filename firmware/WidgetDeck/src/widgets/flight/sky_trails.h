#pragma once
#include "sky_data.h"
#include "sky_airports.h"
namespace sky {
constexpr uint32_t TrailWindow=15*60*1000,TrailInterval=4000,TrailGap=20000;
constexpr uint32_t AutoTrackLostTimeout=60000,AutoTrackResumeDelay=30000;
constexpr unsigned TrailCapacity=240;
struct TrailPoint {float east=0,north=0;uint32_t at=0;};
inline TrailPoint trailPosition(const Aircraft &a,uint32_t received){
  float angle=a.bearing*float(Pi)/180;
  return {a.distance*sinf(angle),a.distance*cosf(angle),received-uint32_t(std::max(0.f,a.age)*1000)};
}
struct Trail {
  Aircraft aircraft;TrailPoint points[TrailCapacity];
  unsigned start=0,count=0;uint32_t lastSeen=0;
  const TrailPoint &point(unsigned i)const{return points[(start+i)%TrailCapacity];}
  TrailPoint &point(unsigned i){return points[(start+i)%TrailCapacity];}
  void prune(uint32_t now){while(count&&uint32_t(now-point(0).at)>TrailWindow){start=(start+1)%TrailCapacity;--count;}}
  void observe(const Aircraft &a,uint32_t received,uint32_t now){
    TrailPoint p=trailPosition(a,received);
    if(count&&int32_t(p.at-lastSeen)<0)return;
    aircraft=a;lastSeen=p.at;prune(now);
    if(count){
      TrailPoint &last=point(count-1);
      // Ignore identical positions instead of drawing a pile of repeated dots.
      if(fabsf(last.east-p.east)<.0001f&&fabsf(last.north-p.north)<.0001f){last.at=p.at;return;}
      if(uint32_t(p.at-last.at)<TrailInterval)return;
    }
    if(count==TrailCapacity){start=(start+1)%TrailCapacity;--count;}
    point(count++)=p;
  }
};
inline bool circling(const Trail &trail,uint32_t now,const AirportZones *airports=nullptr){
  if(trail.count<12||uint32_t(now-trail.lastSeen)>15000)return false;
  TrailPoint current=trailPosition(trail.aircraft,trail.lastSeen);
  if(airports&&airports->contains(current.east,current.north))return false;
  unsigned first=0;
  for(unsigned i=0;i<trail.count;i++){
    const auto &point=trail.point(i);
    // Airport-pattern turns must not be reused as evidence after departure.
    if(uint32_t(now-point.at)>180000||(airports&&airports->contains(point.east,point.north)))first=i+1;
    else if(i&&uint32_t(trail.point(i).at-trail.point(i-1).at)>TrailGap)first=i;
  }
  if(trail.count-first<12)return false;
  const auto &begin=trail.point(first);TrailPoint previous=begin;
  float length=0,signedTurn=0,absoluteTurn=0,lastDx=0,lastDy=0;unsigned segments=0;
  float minE=begin.east,maxE=begin.east,minN=begin.north,maxN=begin.north;
  for(unsigned i=first+1;i<trail.count;i++){
    const auto &p=trail.point(i);float dx=p.east-previous.east,dy=p.north-previous.north,step=hypotf(dx,dy);
    if(step<.015f)continue; // About 80 feet: reject position noise and hovering.
    uint32_t dt=uint32_t(p.at-previous.at);if(!dt||step*3600000.f/dt>900)return false;
    length+=step;
    if(segments){float turn=atan2f(lastDx*dy-lastDy*dx,lastDx*dx+lastDy*dy);signedTurn+=turn;absoluteTurn+=fabsf(turn);}
    ++segments;lastDx=dx;lastDy=dy;previous=p;
    minE=std::min(minE,p.east);maxE=std::max(maxE,p.east);minN=std::min(minN,p.north);maxN=std::max(maxN,p.north);
  }
  uint32_t duration=uint32_t(previous.at-begin.at);
  float turn=fabsf(signedTurn),direct=hypotf(previous.east-begin.east,previous.north-begin.north);
  return segments>=11&&duration>=40000&&length>=.35f&&length*3600000.f/std::max(uint32_t(1),duration)>=20&&
    std::max(maxE-minE,maxN-minN)>=.08f&&turn>=300*float(Pi)/180&&absoluteTurn>0&&turn/absoluteTurn>=.7f&&direct/length<=.45f;
}
class Trails {
  Trail entries[MaxAircraft];char selected[9]={};
  bool autoSelected=false,snoozed=false;uint32_t snoozeUntil=0;
  AirportZones airports;
  void pauseAutomatic(uint32_t now){selected[0]=0;autoSelected=false;snoozed=true;snoozeUntil=now+AutoTrackResumeDelay;}
 public:
  void configureAirports(const Home &home){airports.configure(home);}
  unsigned airportCount()const{return airports.size();}
  bool tracking()const{return selected[0];}
  bool automatic()const{return tracking()&&autoSelected;}
  const char *selectedHex()const{return selected;}
  void reset(){for(auto &e:entries){e.aircraft=Aircraft();e.start=0;e.count=0;e.lastSeen=0;}selected[0]=0;autoSelected=false;snoozed=false;}
  void leave(uint32_t now){selected[0]=0;autoSelected=false;snoozed=true;snoozeUntil=now+600000;}
  bool expireAutomatic(uint32_t now){
    if(!automatic())return false;
    const Trail *trail=current();
    if(trail&&uint32_t(now-trail->lastSeen)<AutoTrackLostTimeout)return false;
    // End unattended tracking on elapsed observation time, even when the feed
    // is offline or this widget is hidden. Manual selections stay pinned.
    pauseAutomatic(now);
    return true;
  }
  bool excludeAirportTracking(uint32_t now){
    if(!automatic())return false;
    const Trail *trail=current();if(!trail)return false;
    TrailPoint point=trailPosition(trail->aircraft,trail->lastSeen);
    if(!airports.contains(point.east,point.north))return false;
    pauseAutomatic(now);return true;
  }
  Trail *find(const char *hex){for(auto &e:entries)if(e.aircraft.hex[0]&&!strcmp(e.aircraft.hex,hex))return &e;return nullptr;}
  const Trail *current()const{for(const auto &e:entries)if(selected[0]&&!strcmp(e.aircraft.hex,selected))return &e;return nullptr;}
  bool select(const char *hex,bool automatic=false){if(!find(hex))return false;snprintf(selected,sizeof(selected),"%s",hex);autoSelected=automatic;return true;}
  bool autoTrack(uint32_t now){
    if(tracking()||(snoozed&&int32_t(now-snoozeUntil)<0))return false;
    const Trail *best=nullptr;for(const auto &e:entries)if(circling(e,now,&airports)&&(!best||e.aircraft.distance<best->aircraft.distance))best=&e;
    return best&&select(best->aircraft.hex,true);
  }
  void prune(uint32_t now){for(auto &e:entries)e.prune(now);}
  void update(const Snapshot &snapshot,uint32_t now){
    // Error snapshots retain earlier observations. Never sample them again.
    if(snapshot.status!=Status::Live)return;
    for(int i=0;i<snapshot.count;i++){
      const Aircraft &a=snapshot.aircraft[i];
      if(!a.hex[0]||!isfinite(a.distance)||a.distance<0||!isfinite(a.bearing)||!isfinite(a.age)||a.age<0||a.age+uint32_t(now-snapshot.received)/1000.f>MaxAge)continue;
      Trail *slot=find(a.hex);
      if(!slot){
        for(auto &e:entries)if(!e.aircraft.hex[0]){slot=&e;break;}
        if(!slot)for(auto &e:entries){
          if(selected[0]&&!strcmp(e.aircraft.hex,selected))continue;
          if(!slot||uint32_t(now-e.lastSeen)>uint32_t(now-slot->lastSeen))slot=&e;
        }
        if(!slot)continue;slot->aircraft=Aircraft();slot->start=0;slot->count=0;
      }
      slot->observe(a,snapshot.received,now);
    }
  }
};
struct TrailBounds {float east=0,north=0,span=.5f;};
inline TrailBounds trailBounds(const Trail &trail){
  TrailPoint current=trailPosition(trail.aircraft,trail.lastSeen);
  float minE=current.east,maxE=current.east,minN=current.north,maxN=current.north;
  for(unsigned i=0;i<trail.count;i++){const auto &p=trail.point(i);minE=std::min(minE,p.east);maxE=std::max(maxE,p.east);minN=std::min(minN,p.north);maxN=std::max(maxN,p.north);}
  float span=std::max(.5f,std::max(maxE-minE,maxN-minN)*1.25f);
  float east=(minE+maxE)/2,north=(minN+maxN)/2;
  // Include home when it is nearby; do not flatten a small distant orbit just
  // to fit the entire distance back home. The renderer points toward home.
  if(fabsf(east)<span*.65f&&fabsf(north)<span*.65f){
    minE=std::min(0.f,minE);maxE=std::max(0.f,maxE);minN=std::min(0.f,minN);maxN=std::max(0.f,maxN);
    east=(minE+maxE)/2;north=(minN+maxN)/2;span=std::max(.5f,std::max(maxE-minE,maxN-minN)*1.25f);
  }
  return {east,north,span};
}
}
