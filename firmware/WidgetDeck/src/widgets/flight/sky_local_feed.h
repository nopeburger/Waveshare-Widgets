#pragma once
#include "sky_feed.h"
#include <ctype.h>
namespace sky {
// Malcolm Robb dump1090's /data.json is always feet/knots/feet-per-minute,
// including when --metric changes its interactive display.
inline bool parseLocalFeed(const char *json,size_t length,const Home &home,uint32_t now,Snapshot &out){
  if(!json||!length||length>128*1024||!validHome(home))return false;
  const char *end=nullptr;cJSON *root=cJSON_ParseWithLengthOpts(json,length,&end,0);if(!root)return false;
  while(end<json+length&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'))++end;
  if(end!=json+length||!cJSON_IsArray(root)){cJSON_Delete(root);return false;}
  Snapshot result;result.status=Status::Live;result.localSource=true;result.received=now;snprintf(result.message,sizeof(result.message),"LOCAL RECEIVER");
  const cJSON *entry;
  cJSON_ArrayForEach(entry,root){
    if(!cJSON_IsObject(entry)||number(entry,"validposition")!=1)continue;
    double lat=number(entry,"lat"),lon=number(entry,"lon"),seen=number(entry,"seen");
    if(!isfinite(lat)||!isfinite(lon)||lat<-90||lat>90||lon<-180||lon>180||!isfinite(seen)||seen<0||seen>MaxAge)continue;
    const cJSON *address=cJSON_GetObjectItemCaseSensitive(entry,"hex");
    if(!cJSON_IsString(address)||!address->valuestring||strlen(address->valuestring)!=6)continue;
    bool hex=true;for(const char *p=address->valuestring;*p;p++)if(!isxdigit((unsigned char)*p))hex=false;if(!hex)continue;
    Aircraft aircraft;field(entry,"hex",aircraft.hex,sizeof(aircraft.hex));
    aircraft.distance=distanceMiles(home,lat,lon);if(aircraft.distance>home.radius)continue;
    aircraft.bearing=bearingDegrees(home,lat,lon);aircraft.age=float(seen);
    field(entry,"flight",aircraft.flight,sizeof(aircraft.flight));
    aircraft.altitude=float(number(entry,"altitude"));aircraft.speed=float(number(entry,"speed"));aircraft.verticalRate=float(number(entry,"vert_rate"));
    if(number(entry,"validtrack")==1)aircraft.track=float(number(entry,"track"));
    if(isfinite(aircraft.altitude)&&(aircraft.altitude< -2000||aircraft.altitude>100000))aircraft.altitude=NAN;
    if(isfinite(aircraft.speed)&&(aircraft.speed<0||aircraft.speed>1500))aircraft.speed=NAN;
    if(isfinite(aircraft.track)&&(aircraft.track<0||aircraft.track>=360))aircraft.track=NAN;
    if(isfinite(aircraft.verticalRate)&&fabsf(aircraft.verticalRate)>20000)aircraft.verticalRate=NAN;
    bool duplicate=false;for(int i=0;i<result.count;i++)if(!strcmp(result.aircraft[i].hex,aircraft.hex))duplicate=true;if(duplicate)continue;
    int pos=0;while(pos<result.count&&(result.aircraft[pos].distance<aircraft.distance||(result.aircraft[pos].distance==aircraft.distance&&strcmp(result.aircraft[pos].hex,aircraft.hex)<0)))++pos;
    if(pos>=MaxAircraft)continue;
    for(int i=std::min(result.count,MaxAircraft-1);i>pos;--i)result.aircraft[i]=result.aircraft[i-1];
    result.aircraft[pos]=aircraft;if(result.count<MaxAircraft)++result.count;
  }
  cJSON_Delete(root);out=result;return true;
}
}
