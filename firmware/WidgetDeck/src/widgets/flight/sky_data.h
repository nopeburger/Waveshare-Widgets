#pragma once
#include <stdint.h>
#include "../../core/location.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>
#include <initializer_list>

namespace sky {
constexpr int Width=280, Height=456, MaxAircraft=24;
constexpr uint32_t SlideMs=10000;
constexpr float MaxAge=60;
constexpr double Pi=3.14159265358979323846;
struct Aircraft {
  char hex[9]={}, flight[16]={}, registration[16]={}, type[8]={}, category[4]={};
  float altitude=NAN, speed=NAN, track=NAN, verticalRate=NAN;
  float distance=0, bearing=0, age=0;
  uint32_t dbFlags=0;
  bool medicalPriority=false;
};
enum class Status { Waiting, Live, Offline, Clock, Error, Setup };
struct Snapshot {
  Aircraft aircraft[MaxAircraft];
  int count=0;
  uint32_t received=0, revision=0, locationRevision=0;
  Status status=Status::Waiting;
  bool localSource=false;
  char message[48]="CONNECTING";
};
inline const char *model(const Aircraft &a) {
  struct Entry {const char *code,*name;};
  static const Entry models[]={
    {"B737","BOEING 737-700"},{"B738","BOEING 737-800"},{"B739","BOEING 737-900"},
    {"B38M","BOEING 737 MAX 8"},{"B39M","BOEING 737 MAX 9"},{"B744","BOEING 747-400"},
    {"B748","BOEING 747-8"},{"B752","BOEING 757-200"},{"B763","BOEING 767-300"},
    {"B772","BOEING 777-200"},{"B77W","BOEING 777-300ER"},{"B788","BOEING 787-8"},
    {"B789","BOEING 787-9"},{"B78X","BOEING 787-10"},{"A319","AIRBUS A319"},
    {"A320","AIRBUS A320"},{"A321","AIRBUS A321"},{"A20N","AIRBUS A320NEO"},
    {"A21N","AIRBUS A321NEO"},{"A332","AIRBUS A330-200"},{"A333","AIRBUS A330-300"},
    {"A359","AIRBUS A350-900"},{"A35K","AIRBUS A350-1000"},{"A388","AIRBUS A380"},
    {"E170","EMBRAER 170"},{"E175","EMBRAER 175"},{"E75L","EMBRAER 175"},
    {"E75S","EMBRAER 175"},{"E190","EMBRAER 190"},{"CRJ7","BOMBARDIER CRJ700"},
    {"CRJ9","BOMBARDIER CRJ900"},{"C172","CESSNA SKYHAWK"},{"C182","CESSNA SKYLANE"},
    {"SR22","CIRRUS SR22"},{"PC12","PILATUS PC-12"},{"C208","CESSNA CARAVAN"},
    {"BE20","BEECHCRAFT KING AIR"},{"H60","SIKORSKY H-60"},{"R44","ROBINSON R44"},
    {"AS50","AIRBUS H125"},{"EC35","AIRBUS H135"},{"B06","BELL 206"},
    {"EC45","AIRBUS H145"},{"H145","AIRBUS H145"},{"S70","SIKORSKY S-70"},
    {"B407","BELL 407"},{"B412","BELL 412"},{"C206","CESSNA STATIONAIR"},
    {"GA8","GIPPSAERO AIRVAN"},{"BE30","BEECHCRAFT KING AIR"},{"C130","LOCKHEED C-130"}
  };
  for(const auto &m:models)if(!strcmp(a.type,m.code))return m.name;
  return a.type[0]?a.type:"AIRCRAFT TYPE UNKNOWN";
}
// Silhouette classes, in the order drawn by Canvas::plane().
enum Shape { NarrowJet=0, LightProp=1, Helicopter=2, WideJet=3, QuadJet=4, RegionalJet=5, BusinessJet=6, Turboprop=7, Transport=8 };
inline bool typeIn(const Aircraft &a,std::initializer_list<const char *> types) {
  for(const char *type:types)if(!strcmp(a.type,type))return true;
  return false;
}
inline int silhouette(const Aircraft &a) {
  if(!strcmp(a.category,"A7"))return Helicopter;
  if(typeIn(a,{"R22","R44","R66","AS50","AS55","AS65","EC30","EC35","EC45","H145","H60","S70","S76","S92","S64","B06","B407","B412","B429","B430","B212","B205","B47","H47","H53","A109","A119","A139","A169","A189","MD52","MD60"}))return Helicopter;
  if(typeIn(a,{"B741","B742","B743","B744","B74S","B748","A342","A343","A345","A346","A388","A124","IL96","IL76","C17","K35R","B52","B703"}))return QuadJet;
  if(typeIn(a,{"C130","C30J","P3","L100","A400","AN12"}))return Transport;
  if(typeIn(a,{"A306","A30B","A310","A332","A333","A338","A339","A359","A35K","B762","B763","B764","B772","B773","B77L","B77W","B788","B789","B78X","MD11","DC10","L101"}))return WideJet;
  if(typeIn(a,{"E135","E145","E170","E75L","E75S","E175","E190","E195","E290","E295","CRJ1","CRJ2","CRJ7","CRJ9","CRJX"}))return RegionalJet;
  if(typeIn(a,{"C25A","C25B","C25C","C25M","C500","C501","C510","C525","C550","C560","C56X","C680","C68A","C700","C750","CL30","CL35","CL60","GLF2","GLF3","GLF4","GLF5","GLF6","GALX","G150","G280","FA10","FA20","FA50","FA7X","FA8X","F900","F2TH","LJ31","LJ35","LJ40","LJ45","LJ55","LJ60","LJ70","LJ75","E50P","E55P","EA50","SF50","HDJT","H25A","H25B","PRM1","ASTR","BE40","PC24"}))return BusinessJet;
  if(typeIn(a,{"AT43","AT44","AT45","AT46","AT72","AT73","AT75","AT76","DH8A","DH8B","DH8C","DH8D","SF34","JS31","JS32","JS41","B190","BE20","BE30","BE9L","BE10","BE99","SW4","E120","DHC6","D328","F50","F27","BE58","PA31","PA34","C310","C340","C414","C421","DA42","P180"}))return Turboprop;
  if(typeIn(a,{"C150","C152","C172","C182","C206","C208","C210","SR20","SR22","PC12","PC6","GA8","PA28","PA32","PA46","AT8T","CL2T"}))return LightProp;
  // No known type: use the ADS-B emitter category (A1 light, A2 small, A3 large, A4 high-vortex, A5 heavy).
  if(!strcmp(a.category,"A1"))return LightProp;
  if(!strcmp(a.category,"A2"))return RegionalJet;
  if(!strcmp(a.category,"A5"))return WideJet;
  return NarrowJet;
}
inline const char *callsign(const Aircraft &a) {return a.flight[0]?a.flight:a.registration[0]?a.registration:a.hex;}

class Slideshow {
 public:
  Aircraft aircraft[MaxAircraft]; int count=0,index=0; bool held=false;
  uint32_t switched=0, received=0;
  void update(const Snapshot &snapshot,uint32_t now) {
    char selected[9]={}; if(count)snprintf(selected,sizeof(selected),"%s",aircraft[index].hex);
    int nextCount=0,found=-1;
    for(int i=0;i<snapshot.count;i++) {
      const auto &a=snapshot.aircraft[i];
      if(a.age+uint32_t(now-snapshot.received)/1000.f>MaxAge)continue;
      aircraft[nextCount]=a;
      if(!strcmp(a.hex,selected))found=nextCount;
      ++nextCount;
    }
    bool lost=selected[0]&&found<0;
    count=nextCount;received=snapshot.received;
    if(found>=0)index=found;
    else {index=0;held=false;switched=now;}
    if(lost)held=false;
    if(count>1&&!held&&uint32_t(now-switched)>=SlideMs)advance(1,now);
  }
  void advance(int direction,uint32_t now) {if(count)index=(index+direction+count)%count;switched=now;}
  void toggleHold(uint32_t now) {if(count){held=!held;switched=now;}}
  float progress(uint32_t now)const {return held?1.f:std::min(1.f,uint32_t(now-switched)/float(SlideMs));}
  const Aircraft *current()const {return count?&aircraft[index]:nullptr;}
};

}
