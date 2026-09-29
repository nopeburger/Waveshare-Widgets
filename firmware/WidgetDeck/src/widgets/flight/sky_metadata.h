#pragma once
#include "sky_data.h"
namespace sky {
// Cache only aircraft identity. Public positions/callsigns must never replace
// the receiver's observations, and a public outage must not pause local data.
class Metadata {
  struct Entry {char hex[9]={},registration[16]={},type[8]={},category[4]={};uint32_t flags=0,at=0;};
  static constexpr unsigned Capacity=96;
  static constexpr uint32_t Lifetime=6*60*60*1000;
  Entry entries[Capacity];unsigned cursor=0;
 public:
  void update(const Snapshot &s,uint32_t now){
    for(int i=0;i<s.count;i++){
      const Aircraft &a=s.aircraft[i];if(!a.hex[0])continue;
      Entry *slot=nullptr;for(auto &e:entries)if(!strcmp(e.hex,a.hex)){slot=&e;break;}
      if(!slot){slot=&entries[cursor];cursor=(cursor+1)%Capacity;}
      snprintf(slot->hex,sizeof(slot->hex),"%s",a.hex);
      snprintf(slot->registration,sizeof(slot->registration),"%s",a.registration);
      snprintf(slot->type,sizeof(slot->type),"%s",a.type);
      snprintf(slot->category,sizeof(slot->category),"%s",a.category);
      slot->flags=a.dbFlags;slot->at=now;
    }
  }
  void enrich(Snapshot &s,uint32_t now)const{
    for(int i=0;i<s.count;i++)for(const auto &e:entries){
      Aircraft &a=s.aircraft[i];if(!e.hex[0]||strcmp(e.hex,a.hex)||uint32_t(now-e.at)>Lifetime)continue;
      snprintf(a.registration,sizeof(a.registration),"%s",e.registration);
      snprintf(a.type,sizeof(a.type),"%s",e.type);
      snprintf(a.category,sizeof(a.category),"%s",e.category);
      a.dbFlags=e.flags;break;
    }
  }
};
}
