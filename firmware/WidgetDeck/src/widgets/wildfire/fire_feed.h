#pragma once
#include "fire_data.h"
#include "cJSON.h"

namespace wildfire {
inline const cJSON *get(const cJSON *item,const char *key){return cJSON_GetObjectItemCaseSensitive(item,key);}
inline bool isText(const cJSON *item,const char *value){return cJSON_IsString(item)&&item->valuestring&&!strcmp(item->valuestring,value);}
inline double number(const cJSON *item){return cJSON_IsNumber(item)&&isfinite(item->valuedouble)?item->valuedouble:NAN;}
inline int64_t seconds(const cJSON *item){double n=number(item);return isfinite(n)&&n>0&&n<1e14?int64_t(n/1000):0;}
inline void cleanText(const cJSON *item,char *out,size_t size,const char *fallback){
  if(!size)return;size_t n=0;
  if(cJSON_IsString(item)&&item->valuestring)for(const unsigned char *p=(const unsigned char*)item->valuestring;*p&&n+1<size;p++){
    if(*p>=32&&*p<=126)out[n++]=*p>='a'&&*p<='z'?*p-'a'+'A':*p;
  }
  while(n&&out[n-1]==' ')--n;out[n]=0;if(!n)snprintf(out,size,"%s",fallback);
}
// Statewide requests deliberately contain no saved coordinates. The exact circle is filtered locally.
inline bool queryUrl(unsigned page,char *out,size_t size){
  if(page>=MaxPages)return false;
  int written=snprintf(out,size,"https://services3.arcgis.com/T4QMspbfLg3qTGWY/arcgis/rest/services/WFIGS_Incident_Locations_Current/FeatureServer/0/query?f=json&where=IncidentTypeCategory%%3D%%27WF%%27%%20AND%%20POOState%%3D%%27US-CA%%27%%20AND%%20FireOutDateTime%%20IS%%20NULL&outSR=4326&returnGeometry=true&outFields=IrwinID,IncidentName,IncidentSize,PercentContained,FireDiscoveryDateTime,ModifiedOnDateTime_dt,FireOutDateTime,IncidentTypeCategory,POOCounty,POOState,ActiveFireCandidate&orderByFields=OBJECTID%%20ASC&resultRecordCount=%d&resultOffset=%u",PageSize,page*PageSize);
  return written>=0&&size_t(written)<size;
}
struct Page {int rows=0;bool more=false;};
// Accumulate complete pages into a private snapshot. The caller publishes only after the final page.
inline bool parsePage(const char *json,size_t length,const sky::Home &home,int64_t epoch,Snapshot &out,Page &page){
  if(!json||!length||!sky::validHome(home)||epoch<=0)return false;
  const char *end=nullptr;cJSON *root=cJSON_ParseWithLengthOpts(json,length,&end,0);if(!root)return false;
  while(end<json+length&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'||*end==0))++end;
  const cJSON *features=get(root,"features"),*sr=get(root,"spatialReference"),*more=get(root,"exceededTransferLimit");
  if(end!=json+length||get(root,"error")||!cJSON_IsArray(features)||!isText(get(root,"geometryType"),"esriGeometryPoint")||number(get(sr,"wkid"))!=4326||(more&&!cJSON_IsBool(more))){cJSON_Delete(root);return false;}
  Page info;info.rows=cJSON_GetArraySize(features);info.more=cJSON_IsTrue(more);
  if(info.rows>PageSize||(info.more&&!info.rows)){cJSON_Delete(root);return false;}
  Snapshot result=out;const cJSON *feature;
  cJSON_ArrayForEach(feature,features){
    const cJSON *a=get(feature,"attributes"),*g=get(feature,"geometry"),*id=get(a,"IrwinID"),*outTime=get(a,"FireOutDateTime");
    if(!isText(get(a,"IncidentTypeCategory"),"WF")||!isText(get(a,"POOState"),"US-CA")||number(get(a,"ActiveFireCandidate"))!=1||(outTime&&!cJSON_IsNull(outTime))||!cJSON_IsString(id)||!id->valuestring||!id->valuestring[0]||strlen(id->valuestring)>=sizeof(Report::id))continue;
    bool validId=true;for(const unsigned char *p=(const unsigned char*)id->valuestring;*p;p++)if(*p<33||*p>126)validId=false;if(!validId)continue;
    double lat=number(get(g,"y")),lon=number(get(g,"x"));if(!isfinite(lat)||!isfinite(lon)||fabs(lat)>90||fabs(lon)>180)continue;
    Report report;report.distance=sky::distanceMiles(home,lat,lon);if(report.distance>RadiusMiles)continue;report.bearing=sky::bearingDegrees(home,lat,lon);
    report.discovered=seconds(get(a,"FireDiscoveryDateTime"));report.updated=seconds(get(a,"ModifiedOnDateTime_dt"));
    if(report.discovered>epoch+120||report.updated>epoch+120)continue;
    cleanText(id,report.id,sizeof(report.id),"");cleanText(get(a,"IncidentName"),report.name,sizeof(report.name),"UNNAMED FIRE");cleanText(get(a,"POOCounty"),report.county,sizeof(report.county),"CALIFORNIA");
    report.acres=float(number(get(a,"IncidentSize")));if(report.acres<0||report.acres>100000000)report.acres=NAN;
    report.contained=float(number(get(a,"PercentContained")));if(report.contained<0||report.contained>100)report.contained=NAN;
    int duplicate=-1;for(int i=0;i<result.count;i++)if(!strcmp(result.reports[i].id,report.id))duplicate=i;
    if(duplicate>=0){if(result.reports[duplicate].updated>=report.updated)continue;for(int i=duplicate;i+1<result.count;i++)result.reports[i]=result.reports[i+1];--result.count;}
    int pos=0;while(pos<result.count&&(result.reports[pos].distance<report.distance||(result.reports[pos].distance==report.distance&&strcmp(result.reports[pos].id,report.id)<0)))++pos;
    if(pos>=MaxReports)continue;for(int i=std::min(result.count,MaxReports-1);i>pos;--i)result.reports[i]=result.reports[i-1];result.reports[pos]=report;if(result.count<MaxReports)++result.count;
  }
  cJSON_Delete(root);out=result;page=info;return true;
}
}
