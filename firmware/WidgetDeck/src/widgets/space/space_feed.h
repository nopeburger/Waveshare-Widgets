#pragma once
#include "space_data.h"
#include "cJSON.h"
#include <stdlib.h>

namespace spacewx {
inline const cJSON *get(const cJSON *p,const char *key){return cJSON_GetObjectItemCaseSensitive(p,key);}
inline const char *text(const cJSON *p){return cJSON_IsString(p)&&p->valuestring?p->valuestring:"";}
inline double number(const cJSON *p){
  if(cJSON_IsNumber(p))return isfinite(p->valuedouble)?p->valuedouble:NAN;
  if(!cJSON_IsString(p)||!p->valuestring||!p->valuestring[0])return NAN;
  char *end=nullptr;double value=strtod(p->valuestring,&end);
  return end!=p->valuestring&&!*end&&isfinite(value)?value:NAN;
}
inline int scale(const cJSON *p){double n=number(p);return isfinite(n)&&n>=0&&n<=5&&floor(n)==n?int(n):-1;}
inline bool leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
// NOAA uses UTC with either a space or T separator, optional fractions and Z.
// Parse explicitly; local timezone/DST must never shift observation freshness.
inline int64_t timestamp(const char *s){
  size_t n=strlen(s);if(n<19||s[4]!='-'||s[7]!='-'||(s[10]!='T'&&s[10]!=' ')||s[13]!=':'||s[16]!=':')return 0;
  const int slots[]={0,1,2,3,5,6,8,9,11,12,14,15,17,18};for(int i:slots)if(s[i]<'0'||s[i]>'9')return 0;
  auto two=[&](int i){return (s[i]-'0')*10+s[i+1]-'0';};
  int y=two(0)*100+two(2),m=two(5),d=two(8),h=two(11),min=two(14),sec=two(17);
  static const int months[]={31,28,31,30,31,30,31,31,30,31,30,31};
  if(y<1970||y>2099||m<1||m>12||d<1||d>months[m-1]+(m==2&&leap(y))||h>23||min>59||sec>59)return 0;
  size_t end=19;if(s[end]=='.'){size_t start=++end;while(s[end]>='0'&&s[end]<='9')++end;if(start==end)return 0;}
  if(s[end]=='Z')++end;if(end!=n)return 0;
  int64_t days=0;for(int year=1970;year<y;year++)days+=leap(year)?366:365;
  for(int month=1;month<m;month++)days+=months[month-1]+(month==2&&leap(y));
  return ((days+d-1)*24+h)*3600+min*60+sec;
}
struct Document {
  cJSON *root=nullptr;
  Document(const char *json,size_t length){
    if(!json||!length)return;const char *end=nullptr;root=cJSON_ParseWithLengthOpts(json,length,&end,0);if(!root)return;
    while(end<json+length&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'))++end;
    if(end!=json+length){cJSON_Delete(root);root=nullptr;}
  }
  ~Document(){cJSON_Delete(root);}
  Document(const Document&)=delete;Document &operator=(const Document&)=delete;
};
inline int64_t scaleStamp(const cJSON *p){
  const char *date=text(get(p,"DateStamp")),*clock=text(get(p,"TimeStamp"));
  if(strlen(date)!=10||strlen(clock)!=8)return 0;char stamp[21];snprintf(stamp,sizeof(stamp),"%sT%s",date,clock);return timestamp(stamp);
}
inline bool parseScales(const char *json,size_t length,int64_t epoch,Scales &out){
  Document doc(json,length);const cJSON *current=get(doc.root,"0");
  int64_t stamp=scaleStamp(current);if(!cJSON_IsObject(current)||!timely(stamp,epoch))return false;
  Scales result;result.stamp=stamp;
  result.g=scale(get(get(current,"G"),"Scale"));result.s=scale(get(get(current,"S"),"Scale"));result.r=scale(get(get(current,"R"),"Scale"));
  for(int i=0;i<3;i++){
    char key[2]={char('1'+i),0};const cJSON *forecast=get(doc.root,key);int64_t date=scaleStamp(forecast);
    if(!date)continue;int64_t day=date/86400,offset=day-epoch/86400;
    if(offset!=i)continue;result.outlook[i].day=day;result.outlook[i].g=scale(get(get(forecast,"G"),"Scale"));
  }
  out=result;return true;
}
inline bool parseKp(const char *json,size_t length,int64_t epoch,Kp &out){
  Document doc(json,length);if(!cJSON_IsArray(doc.root))return false;
  Kp result;const cJSON *row;
  // First find the actual newest observation, including an explicitly missing value.
  cJSON_ArrayForEach(row,doc.root){
    int64_t stamp=timestamp(text(get(row,"time_tag")));if(!stamp||stamp>epoch+120||epoch-stamp>HistorySeconds)continue;
    double value=number(get(row,"estimated_kp"));if(!isfinite(value)||value<0||value>9)value=NAN;
    if(stamp>=result.stamp){result.stamp=stamp;result.value=float(value);}
  }
  if(!timely(result.stamp,epoch))return false;
  Sample bins[HistoryBins];int64_t firstBin=result.stamp/300-(HistoryBins-1);
  cJSON_ArrayForEach(row,doc.root){
    int64_t stamp=timestamp(text(get(row,"time_tag")));int64_t bin=stamp/300-firstBin;
    if(!stamp||stamp>result.stamp||bin<0||bin>=HistoryBins)continue;
    double value=number(get(row,"estimated_kp"));if(!isfinite(value)||value<0||value>9)continue;
    if(stamp>=bins[bin].stamp)bins[bin]={stamp,float(value)};
  }
  for(const auto &sample:bins)if(sample.stamp)result.samples[result.count++]=sample;
  out=result;return true;
}
inline bool parseWind(const char *json,size_t length,int64_t epoch,Wind &out){
  Document doc(json,length);if(!cJSON_IsArray(doc.root)&&!cJSON_IsObject(doc.root))return false;
  Wind result;
  auto accept=[&](const cJSON *row){
    int64_t stamp=timestamp(text(get(row,"time_tag")));if(!timely(stamp,epoch,WindFreshSeconds)||stamp<result.stamp)return;
    double speed=number(get(row,"proton_speed"));
    result.stamp=stamp;result.mph=isfinite(speed)&&speed>=100&&speed<=3000?float(speed*2236.9362920544):NAN;
  };
  if(cJSON_IsObject(doc.root))accept(doc.root);else{const cJSON *row;cJSON_ArrayForEach(row,doc.root)accept(row);}
  if(!result.stamp)return false;out=result;return true;
}
}
