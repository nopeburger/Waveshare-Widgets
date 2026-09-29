#pragma once
#include "quake_data.h"
#include "cJSON.h"
#include <stdlib.h>

namespace quake {
inline const cJSON *get(const cJSON *item,const char *key){return cJSON_GetObjectItemCaseSensitive(item,key);}
inline bool isText(const cJSON *item,const char *value){return cJSON_IsString(item)&&item->valuestring&&!strcmp(item->valuestring,value);}
inline double number(const cJSON *item){return cJSON_IsNumber(item)&&isfinite(item->valuedouble)?item->valuedouble:NAN;}
inline int64_t seconds(const cJSON *item){double n=number(item);return isfinite(n)&&n>0&&n<1e14?int64_t(n/1000):0;}
inline void placeText(const cJSON *item,char *out,size_t size){
  if(!out||!size)return;
  size_t n=0;
  if(cJSON_IsString(item)&&item->valuestring){
    const char *source=item->valuestring;while(*source==' ')++source;
    char *unit=nullptr;double kilometers=strtod(source,&unit);while(*unit==' ')++unit;
    // USGS descriptions begin with a distance such as "3 km NNW of ...".
    if(unit!=source&&isfinite(kilometers)&&kilometers>=0&&(unit[0]=='k'||unit[0]=='K')&&(unit[1]=='m'||unit[1]=='M')&&(unit[2]==' '||unit[2]==0)){
      n=std::min(size-1,size_t(snprintf(out,size,"%.1f MI",milesFromKm(kilometers))));source=unit+2;
    }
    for(const unsigned char *p=(const unsigned char*)source;*p&&n+1<size;++p){
      if(*p>=32&&*p<=126)out[n++]=*p>='a'&&*p<='z'?*p-'a'+'A':*p;
    }
  }
  while(n&&out[n-1]==' ')--n;out[n]=0;if(!n)snprintf(out,size,"LOCATION NOT REPORTED");
}
inline bool parseFeed(const char *json,size_t length,const sky::Home &home,int64_t epoch,Snapshot &out){
  if(!json||!length||!sky::validHome(home)||epoch<=0)return false;
  const char *end=nullptr;cJSON *root=cJSON_ParseWithLengthOpts(json,length,&end,0);if(!root)return false;
  while(end<json+length&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'||*end==0))++end;
  const cJSON *metadata=get(root,"metadata"),*features=get(root,"features");
  int64_t generated=seconds(get(metadata,"generated"));
  if(end!=json+length||!isText(get(root,"type"),"FeatureCollection")||!cJSON_IsArray(features)||!generated||generated>epoch+120||epoch-generated>FreshSeconds||(get(metadata,"status")&&number(get(metadata,"status"))!=200)){cJSON_Delete(root);return false;}
  Snapshot result;result.lat=home.lat;result.lon=home.lon;result.generated=generated;result.status=Status::Live;snprintf(result.message,sizeof(result.message),"LIVE");
  const cJSON *feature;
  cJSON_ArrayForEach(feature,features){
    const cJSON *properties=get(feature,"properties"),*geometry=get(feature,"geometry"),*id=get(feature,"id");
    if(!isText(get(feature,"type"),"Feature")||!isText(get(properties,"type"),"earthquake")||isText(get(properties,"status"),"deleted")||!isText(get(geometry,"type"),"Point")||!cJSON_IsString(id)||!id->valuestring||!id->valuestring[0]||strlen(id->valuestring)>=sizeof(Event::id))continue;
    bool validId=true;for(const unsigned char *p=(const unsigned char*)id->valuestring;*p;p++)if(*p<33||*p>126)validId=false;if(!validId)continue;
    const cJSON *coordinates=get(geometry,"coordinates");if(!cJSON_IsArray(coordinates)||cJSON_GetArraySize(coordinates)<3)continue;
    double lon=number(cJSON_GetArrayItem(coordinates,0)),lat=number(cJSON_GetArrayItem(coordinates,1));
    if(!isfinite(lat)||!isfinite(lon)||fabs(lat)>90||fabs(lon)>180)continue;
    Event event;event.occurred=seconds(get(properties,"time"));if(!inWindow(event,epoch))continue;
    event.distance=sky::distanceMiles(home,lat,lon);if(event.distance>RadiusMiles)continue;
    event.bearing=sky::bearingDegrees(home,lat,lon);event.updated=seconds(get(properties,"updated"));
    snprintf(event.id,sizeof(event.id),"%s",id->valuestring);placeText(get(properties,"place"),event.place,sizeof(event.place));
    event.magnitude=float(number(get(properties,"mag")));if(event.magnitude<-10||event.magnitude>12)event.magnitude=NAN;
    event.depthKm=float(number(cJSON_GetArrayItem(coordinates,2)));if(event.depthKm<-100||event.depthKm>1000)event.depthKm=NAN;
    event.reviewed=isText(get(properties,"status"),"reviewed");
    int duplicate=-1;for(int i=0;i<result.count;i++)if(!strcmp(result.events[i].id,event.id))duplicate=i;
    if(duplicate>=0){if(result.events[duplicate].updated>=event.updated)continue;for(int i=duplicate;i+1<result.count;i++)result.events[i]=result.events[i+1];--result.count;}
    int pos=0;while(pos<result.count&&(result.events[pos].occurred>event.occurred||(result.events[pos].occurred==event.occurred&&strcmp(result.events[pos].id,event.id)<0)))++pos;
    if(pos>=MaxEvents)continue;
    for(int i=std::min(result.count,MaxEvents-1);i>pos;--i)result.events[i]=result.events[i-1];
    result.events[pos]=event;if(result.count<MaxEvents)++result.count;
  }
  cJSON_Delete(root);out=result;return true;
}
}
