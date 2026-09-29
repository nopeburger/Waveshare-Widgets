#pragma once
#include <stdint.h>
#include "../../core/location.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <algorithm>

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
inline int silhouette(const Aircraft &a) {
  if(!strcmp(a.category,"A7"))return 2;
  static const char *helicopters[]={"R22","R44","R66","AS50","AS55","AS65","EC30","EC35","EC45","H145","H60","S70","S76","S92","S64","B06","B407","B412","B429","B430","B212","B205","B47","H47","H53","A109","A119","A139","A169","A189","MD52","MD60"};
  for(const char *type:helicopters)if(!strcmp(a.type,type))return 2;
  static const char *props[]={"C150","C152","C172","C182","C206","C208","C210","C310","C340","C414","C421","SR20","SR22","PC12","PC6","BE20","BE30","BE9L","BE10","BE58","C130","C30J","P3","DHC6","DH8A","DH8B","DH8C","DH8D","AT43","AT45","AT46","AT72","AT73","AT75","AT76","GA8","PA28","PA31","PA32","PA34","PA46","AT8T","CL2T"};
  for(const char *type:props)if(!strcmp(a.type,type))return 1;
  // A1 means light weight, which can also be a jet (e.g. a Citation).
  static const char *lightJets[]={"C510","C525","C25A","C25B","C25C","EA50","SF50","HDJT"};
  for(const char *type:lightJets)if(!strcmp(a.type,type))return 0;
  if(!strcmp(a.category,"A1"))return 1;
  return 0;
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
