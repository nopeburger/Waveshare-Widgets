#pragma once
#include "sky_data.h"
#include "cJSON.h"

namespace sky {
inline double number(const cJSON *item,const char *key,double fallback=NAN) {
  const cJSON *v=cJSON_GetObjectItemCaseSensitive(item,key);
  return cJSON_IsNumber(v)&&isfinite(v->valuedouble)?v->valuedouble:fallback;
}
inline void field(const cJSON *item,const char *key,char *out,size_t capacity) {
  const cJSON *v=cJSON_GetObjectItemCaseSensitive(item,key);size_t n=0;
  if(cJSON_IsString(v)&&v->valuestring)for(const char *p=v->valuestring;*p&&n+1<capacity;p++) {
    unsigned char c=*p;
    if(c>=32&&c<=126)out[n++]=(c>='a'&&c<='z')?c-32:c;
  }
  while(n&&out[n-1]==' ')--n;
  out[n]=0;
}
inline bool parseFeed(const char *json,size_t length,const Home &home,double epochSeconds,uint32_t now,Snapshot &out) {
  if(!validHome(home)||!isfinite(epochSeconds))return false;
  // Require an entire JSON document; a truncated download must not become an empty sky.
  const char *end=nullptr;
  cJSON *root=cJSON_ParseWithLengthOpts(json,length,&end,0);
  if(!root)return false;
  while(end<json+length&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'||*end==0))++end;
  const cJSON *list=cJSON_GetObjectItemCaseSensitive(root,"ac");
  double feedTime=number(root,"now")/1000.0;
  if(end!=json+length||!cJSON_IsArray(list)||!isfinite(feedTime)||feedTime>epochSeconds+120||epochSeconds-feedTime>MaxAge) {cJSON_Delete(root);return false;}
  Snapshot result;result.received=now;result.status=Status::Live;
  snprintf(result.message,sizeof(result.message),"LIVE");
  const cJSON *entry;
  cJSON_ArrayForEach(entry,list) {
    double lat=number(entry,"lat"),lon=number(entry,"lon"),seen=number(entry,"seen_pos");
    if(!isfinite(lat)||!isfinite(lon)||lat<-90||lat>90||lon<-180||lon>180||!isfinite(seen)||seen<0)continue;
    const cJSON *alt=cJSON_GetObjectItemCaseSensitive(entry,"alt_baro");
    if(cJSON_IsString(alt)&&!strcmp(alt->valuestring,"ground"))continue;
    Aircraft a;field(entry,"hex",a.hex,sizeof(a.hex));if(!a.hex[0])continue;
    a.age=float(seen+std::max(0.0,epochSeconds-feedTime));if(a.age>MaxAge)continue;
    a.distance=distanceMiles(home,lat,lon);if(a.distance>home.radius)continue;
    a.bearing=bearingDegrees(home,lat,lon);
    field(entry,"flight",a.flight,sizeof(a.flight));field(entry,"r",a.registration,sizeof(a.registration));
    field(entry,"t",a.type,sizeof(a.type));field(entry,"category",a.category,sizeof(a.category));
    double flags=number(entry,"dbFlags");
    if(isfinite(flags)&&flags>=0&&flags<=UINT32_MAX&&floor(flags)==flags)a.dbFlags=uint32_t(flags);
    const cJSON *emergency=cJSON_GetObjectItemCaseSensitive(entry,"emergency");
    a.medicalPriority=cJSON_IsString(emergency)&&!strcmp(emergency->valuestring,"lifeguard");
    a.altitude=float(number(entry,"alt_baro"));a.speed=float(number(entry,"gs"));
    a.track=float(number(entry,"track"));a.verticalRate=float(number(entry,"baro_rate"));
    if(!isfinite(a.altitude))a.altitude=float(number(entry,"alt_geom"));
    if(isfinite(a.speed)&&(a.speed<0||a.speed>1500))a.speed=NAN;
    if(isfinite(a.track)&&(a.track<0||a.track>=360))a.track=NAN;
    bool duplicate=false;for(int i=0;i<result.count;i++)if(!strcmp(result.aircraft[i].hex,a.hex))duplicate=true;
    if(duplicate)continue;
    int pos=0;while(pos<result.count&&(result.aircraft[pos].distance<a.distance||(result.aircraft[pos].distance==a.distance&&strcmp(result.aircraft[pos].hex,a.hex)<0)))++pos;
    if(pos>=MaxAircraft)continue;
    int last=std::min(result.count,MaxAircraft-1);
    for(int i=last;i>pos;--i)result.aircraft[i]=result.aircraft[i-1];
    result.aircraft[pos]=a;if(result.count<MaxAircraft)++result.count;
  }
  cJSON_Delete(root);out=result;return true;
}
}
