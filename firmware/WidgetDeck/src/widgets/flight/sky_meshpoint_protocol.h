#pragma once
#include "cJSON.h"
#include <stdint.h>
#include <string.h>
#include <ctype.h>
namespace sky {
inline cJSON *meshpointDocument(const char *data,size_t size){
  if(!data||!size||size>128*1024)return nullptr;
  const char *end=nullptr;cJSON *root=cJSON_ParseWithLengthOpts(data,size,&end,0);if(!root)return nullptr;
  while(end<data+size&&(*end==' '||*end=='\r'||*end=='\n'||*end=='\t'))end++;
  if(end!=data+size||!cJSON_IsObject(root)){cJSON_Delete(root);return nullptr;}return root;
}
// Header-safe bearer token, copied only from the configured Pi's login response.
inline bool meshpointToken(const char *data,size_t size,char *out,size_t capacity){
  cJSON *root=meshpointDocument(data,size);if(!root)return false;
  const cJSON *role=cJSON_GetObjectItemCaseSensitive(root,"role"),*token=cJSON_GetObjectItemCaseSensitive(root,"token");
  bool valid=cJSON_IsString(role)&&!strcmp(role->valuestring,"viewer")&&cJSON_IsString(token)&&token->valuestring;
  size_t len=valid?strlen(token->valuestring):0;valid=valid&&len>0&&len<capacity;
  if(valid)for(size_t i=0;i<len;i++)if(!isalnum((unsigned char)token->valuestring[i])&&token->valuestring[i]!='.'&&token->valuestring[i]!='_'&&token->valuestring[i]!='-')valid=false;
  if(valid)memcpy(out,token->valuestring,len+1);cJSON_Delete(root);return valid;
}
inline bool meshpointRunning(const char *data,size_t size,bool &running){
  cJSON *root=meshpointDocument(data,size);if(!root)return false;
  const cJSON *value=cJSON_GetObjectItemCaseSensitive(root,"running");bool valid=cJSON_IsBool(value);
  if(valid)running=cJSON_IsTrue(value);cJSON_Delete(root);return valid;
}
// Use monotonic time, including across millis() rollover and before NTP sync.
struct Keepalive {
  uint32_t due=0,lastGood=0;bool running=false,confirmed=false;
  bool ready(uint32_t now)const{return !due||int32_t(now-due)>=0;}
  bool healthy(uint32_t now)const{return confirmed&&running&&uint32_t(now-lastGood)<90000;}
  void status(bool active,uint32_t now){confirmed=true;running=active;lastGood=now;due=now+30000;}
  void failure(uint32_t now,uint32_t retry=30000,bool invalidate=false){due=now+retry;if(invalidate)confirmed=false;}
};
}
