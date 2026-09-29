#pragma once
#include "location.h"
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <algorithm>

namespace sky {
struct CardSettings {char ssid[33]={},password[64]={};Home home;uint64_t fingerprint=0;};
// Existing saved settings survive upgrades. A subsequently edited SD file wins.
inline bool applyCardSettings(bool saved,bool tracked,uint64_t previous,uint64_t current){return !saved||(tracked&&previous!=current);}
// Values are literal: spaces, # and = inside passwords are preserved.
inline bool parseSettings(const char *data,size_t length,CardSettings &out) {
  if(!data||length>2048)return false;
  for(size_t i=0;i<length;i++)if(data[i]==0)return false;
  CardSettings parsed=out;unsigned seen=0;size_t pos=0;
  if(length>=3&&(unsigned char)data[0]==0xef&&(unsigned char)data[1]==0xbb&&(unsigned char)data[2]==0xbf)pos=3;
  while(pos<length) {
    size_t start=pos;while(pos<length&&data[pos]!='\n')pos++;size_t end=pos;if(pos<length)pos++;
    if(end>start&&data[end-1]=='\r')end--;
    while(start<end&&(data[start]==' '||data[start]=='\t'))start++;
    if(start==end||data[start]=='#'||data[start]==';')continue;
    size_t equal=start;while(equal<end&&data[equal]!='=')equal++;
    if(equal==end)return false;
    size_t keyEnd=equal;while(keyEnd>start&&(data[keyEnd-1]==' '||data[keyEnd-1]=='\t'))keyEnd--;
    char key[24];if(keyEnd-start>=sizeof(key))return false;memcpy(key,data+start,keyEnd-start);key[keyEnd-start]=0;
    const char *value=data+equal+1;size_t size=end-equal-1;unsigned bit=0;
    if(!strcmp(key,"ssid")||!strcmp(key,"password")) {
      bool isSsid=!strcmp(key,"ssid");bit=isSsid?1:2;size_t max=isSsid?32:63;
      if(size>max||(isSsid&&!size)||(!isSsid&&size>0&&size<8))return false;
      char *dest=isSsid?parsed.ssid:parsed.password;memcpy(dest,value,size);dest[size]=0;
    } else {
      char raw[48];if(!size||size>=sizeof(raw))return false;memcpy(raw,value,size);raw[size]=0;
      char *tail;double number=strtod(raw,&tail);if(tail==raw||*tail||!isfinite(number))return false;
      if(!strcmp(key,"latitude")){parsed.home.lat=number;bit=4;}
      else if(!strcmp(key,"longitude")){parsed.home.lon=number;bit=8;}
      else if(!strcmp(key,"radius_miles")){parsed.home.radius=float(number);bit=16;}
      else return false;
    }
    if(seen&bit)return false;seen|=bit;
  }
  if((seen&3)!=3||!validHome(parsed.home)||!strcmp(parsed.ssid,"YOUR_WIFI_NAME"))return false;
  parsed.fingerprint=14695981039346656037ULL;
  for(size_t i=0;i<length;i++){parsed.fingerprint^=static_cast<unsigned char>(data[i]);parsed.fingerprint*=1099511628211ULL;}
  out=parsed;return true;
}
}
