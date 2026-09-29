#pragma once
#include "sky_source_settings.h"
#include "sky_http.h"
#include "sky_meshpoint_protocol.h"
namespace sky {
class MeshPoint {
  char token[1537]={};Keepalive keepalive;
  bool login(const SourceSettings &s){
    cJSON *root=cJSON_CreateObject();if(!root)return false;
    cJSON_AddStringToObject(root,"username","viewer");cJSON_AddStringToObject(root,"password",s.password);
    char *post=cJSON_PrintUnformatted(root);cJSON_Delete(root);if(!post)return false;
    char url[96];snprintf(url,sizeof(url),"http://%s:%u/api/auth/login",s.ip,s.dashboardPort);
    Body body(4096);int code=localRequest(url,body,nullptr,post);memset(post,0,strlen(post));free(post);
    bool ok=code==200&&meshpointToken(body.data,body.length,token,sizeof(token));
    if(!ok){
      // Avoid triggering MeshPoint's login lockout for an incorrect password.
      uint32_t retry=std::max(uint32_t(code==0||code>=500?30000:300000),body.retryAfter);
      keepalive.failure(millis(),retry,true);
      snprintf(error,sizeof(error),"%s",code==401||code==403?"CHECK VIEWER PASSWORD":"VIEWER LOGIN FAILED");
      Serial.printf("MeshPoint viewer login failed (HTTP %d); retry in %us.\n",code,unsigned(retry/1000));
    }
    if(body.data)memset(body.data,0,body.length);return ok;
  }
 public:
  char error[48]="CONNECTING TO MESHPOINT";uint32_t heartbeats=0;
  bool healthy(uint32_t now)const{return keepalive.healthy(now);}
  void tick(const SourceSettings &s){
    if(!keepalive.ready(millis()))return;
    for(int attempt=0;attempt<2;attempt++){
      if(!token[0]&&!login(s))return;
      char url[96],authorization[sizeof(token)+8];snprintf(url,sizeof(url),"http://%s:%u/api/adsb/status",s.ip,s.dashboardPort);
      snprintf(authorization,sizeof(authorization),"Bearer %s",token);
      Body body;int code=localRequest(url,body,authorization);memset(authorization,0,sizeof(authorization));
      if(code==401){
        memset(token,0,sizeof(token));keepalive.failure(millis(),300000,true);strcpy(error,"VIEWER SESSION EXPIRED");
        // One refresh per expired session, then back off if the new token fails.
        if(attempt==0)continue;return;
      }
      bool running=false;
      if(code==200&&meshpointRunning(body.data,body.length,running)){
        keepalive.status(running,millis());++heartbeats;
        snprintf(error,sizeof(error),"%s",running?"LOCAL RECEIVER":"START LISTENING ON PI");
      }else{
        keepalive.failure(millis(),std::max(uint32_t(30000),body.retryAfter),code==403);
        snprintf(error,sizeof(error),"%s",code==403?"VIEWER ACCESS DENIED":"MESHPOINT UNREACHABLE");
      }
      return;
    }
  }
};
}
