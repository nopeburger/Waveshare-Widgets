#pragma once
#include "gas_data.h"
#include "cJSON.h"
#include <cstdlib>

namespace gas {
inline const char *jsonString(const cJSON *o,const char *key){const cJSON *v=cJSON_GetObjectItemCaseSensitive(o,key);return cJSON_IsString(v)?v->valuestring:nullptr;}
inline bool parseFeed(const char *json,size_t length,int today,const char *expectedSeries,History &out) {
  if(!json||!length||length>128*1024||!expectedSeries||!*expectedSeries)return false;
  const char *end=nullptr;cJSON *root=cJSON_ParseWithLengthOpts(json,length,&end,0);if(!root)return false;
  while(end<json+length&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'))end++;
  const cJSON *response=cJSON_GetObjectItemCaseSensitive(root,"response"),*rows=cJSON_GetObjectItemCaseSensitive(response,"data");
  bool valid=end==json+length&&cJSON_IsArray(rows)&&cJSON_GetArraySize(rows)<=MaxPoints&&!cJSON_GetObjectItemCaseSensitive(root,"error");
  History h;const cJSON *row;
  if(valid)cJSON_ArrayForEach(row,rows) {
    const char *series=jsonString(row,"series"),*units=jsonString(row,"units"),*period=jsonString(row,"period");int day;
    if(!series||strcmp(series,expectedSeries)||!units||strcmp(units,"$/GAL")||!parseDate(period,day)){valid=false;break;}
    const cJSON *value=cJSON_GetObjectItemCaseSensitive(row,"value");
    // Missing published observations remain gaps. Never interpolate prices.
    if(cJSON_IsNull(value))continue;
    double price=NAN;
    if(cJSON_IsNumber(value))price=value->valuedouble;
    else if(cJSON_IsString(value)){char *tail;price=strtod(value->valuestring,&tail);if(tail==value->valuestring||*tail)price=NAN;}
    if(!std::isfinite(price)||price<=0||price>100){valid=false;break;}
    h.points[h.count++]={day,float(price)};
  }
  cJSON_Delete(root);if(!valid||!normalize(h,today))return false;out=h;return true;
}
}
