#pragma once
#include "printer_data.h"
#include "cJSON.h"
#include <ctype.h>
#include <stdlib.h>
namespace printer {
inline const cJSON *get(const cJSON *o,const char *key){return cJSON_GetObjectItemCaseSensitive(o,key);}
inline bool number(const cJSON *v,double min,double max,double &out){
  double n;if(cJSON_IsNumber(v))n=v->valuedouble;
  else if(cJSON_IsString(v)&&v->valuestring&&*v->valuestring){char *end;n=strtod(v->valuestring,&end);if(*end)return false;}
  else return false;if(!isfinite(n)||n<min||n>max)return false;out=n;return true;
}
inline void textValue(const cJSON *v,char *out,size_t size,bool uppercase=false){
  if(!cJSON_IsString(v)||!v->valuestring)return;size_t n=0;
  for(const unsigned char *p=(const unsigned char*)v->valuestring;*p&&n+1<size;p++){char c=*p>=32&&*p<=126?char(*p):' ';out[n++]=uppercase?char(toupper((unsigned char)c)):c;}out[n]=0;
}
inline State readState(const char *s){
  if(!strcmp(s,"IDLE"))return State::Idle;
  if(!strcmp(s,"PREPARE"))return State::Preparing;
  if(!strcmp(s,"RUNNING"))return State::Printing;
  if(!strcmp(s,"PAUSE"))return State::Paused;
  if(!strcmp(s,"FINISH"))return State::Finished;
  if(!strcmp(s,"FAILED"))return State::Failed;
  return State::Unknown;
}
inline bool mergeReport(const char *data,size_t size,Snapshot &out,uint32_t now,int64_t epoch){
  if(!data||!size||size>MessageLimit||memchr(data,0,size))return false;
  const char *end=nullptr;cJSON *root=cJSON_ParseWithLengthOpts(data,size,&end,false);
  if(!root)return false;
  while(end<data+size&&isspace((unsigned char)*end))end++;
  const cJSON *p=get(root,"print");
  if(end!=data+size||!cJSON_IsObject(root)||!cJSON_IsObject(p)){cJSON_Delete(root);return false;}
  const cJSON *cmd=get(p,"command");if(cmd&&(!cJSON_IsString(cmd)||strcmp(cmd->valuestring,"push_status"))){cJSON_Delete(root);return false;}
  Snapshot next=out;bool changed=false;
  const cJSON *state=get(p,"gcode_state");State newState=cJSON_IsString(state)?readState(state->valuestring):next.state;
  char job[64]={},started[24]={};textValue(get(p,"subtask_id"),job,sizeof(job));textValue(get(p,"gcode_start_time"),started,sizeof(started));
  bool newJob=(job[0]&&next.job[0]&&strcmp(job,next.job))||(started[0]&&strcmp(started,"0")&&next.started[0]&&strcmp(started,next.started));
  bool wasStopped=next.state==State::Idle||next.state==State::Finished||next.state==State::Failed;
  if(newJob||(wasStopped&&(newState==State::Preparing||newState==State::Printing)))clearJob(next);
  if(next.state==State::Paused&&newState!=State::Paused)next.remainingEpoch=0;
  if(cJSON_IsString(state)){next.state=newState;changed=true;if(newState==State::Idle)clearJob(next);}
  if(cJSON_IsString(get(p,"subtask_id"))){textValue(get(p,"subtask_id"),next.job,sizeof(next.job));changed=true;}
  if(cJSON_IsString(get(p,"gcode_start_time"))){textValue(get(p,"gcode_start_time"),next.started,sizeof(next.started));changed=true;}
  const cJSON *name=get(p,"subtask_name");if(cJSON_IsString(name)){textValue(name,next.name,sizeof(next.name),true);changed=true;}
  double n;
  auto integer=[&](const char *key,int &dest,int max){const cJSON *v=get(p,key);if(!v)return;if(number(v,0,max,n)&&floor(n)==n){dest=int(n);changed=true;}else if(cJSON_IsNull(v)){dest=-1;changed=true;}};
  integer("mc_percent",next.progress,100);integer("layer_num",next.layer,1000000);integer("total_layer_num",next.totalLayers,1000000);
  const cJSON *remaining=get(p,"mc_remaining_time");if(remaining){if(number(remaining,0,1000000,n)&&floor(n)==n){next.remaining=int(n);next.remainingEpoch=epoch;changed=true;}else if(cJSON_IsNull(remaining)){next.remaining=-1;next.remainingEpoch=0;changed=true;}}
  auto temp=[&](const char *key,float &dest){const cJSON *v=get(p,key);if(!v)return;if(number(v,-40,400,n)){dest=float(n);changed=true;}else if(cJSON_IsNull(v)){dest=NAN;changed=true;}};
  temp("nozzle_temper",next.nozzle);temp("nozzle_target_temper",next.nozzleTarget);temp("bed_temper",next.bed);temp("bed_target_temper",next.bedTarget);
  const cJSON *error=get(p,"print_error");if(error&&number(error,0,4294967295.0,n)&&floor(n)==n){next.error=uint32_t(n);changed=true;}
  // Ignore leftover job counters in the idle report; an idle printer has no active job.
  if(next.state==State::Idle)clearJob(next);
  // P1 reports are deltas: an otherwise unchanged status push confirms existing values.
  if(!changed&&cmd&&out.revision)changed=true;
  if(changed){next.received=now;next.revision=out.revision+1;if(!next.revision)next.revision=1;out=next;}
  cJSON_Delete(root);return changed;
}
}
