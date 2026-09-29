#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <algorithm>
namespace printer {
constexpr uint32_t StaleMs=60000,PushIntervalMs=300000;
constexpr size_t MessageLimit=32768;
enum class Link {Setup,InvalidConfig,Disabled,Wifi,Clock,Connecting,Waiting,Live,Offline,Auth,Certificate,Error};
enum class State {Unknown,Idle,Preparing,Printing,Paused,Finished,Failed};
struct Snapshot {
  Link link=Link::Setup;State state=State::Unknown;
  int progress=-1,remaining=-1,layer=-1,totalLayers=-1;
  float nozzle=NAN,nozzleTarget=NAN,bed=NAN,bedTarget=NAN;
  uint32_t error=0,received=0,revision=0;int64_t remainingEpoch=0;
  char name[96]={},job[64]={},started[24]={};
};
inline bool fresh(const Snapshot &s,uint32_t now){return s.link==Link::Live&&s.revision&&uint32_t(now-s.received)<=StaleMs;}
inline bool printing(const Snapshot &s){return s.state==State::Printing||s.state==State::Preparing;}
inline void clearJob(Snapshot &s){s.progress=s.remaining=s.layer=s.totalLayers=-1;s.remainingEpoch=0;s.error=0;s.name[0]=s.job[0]=s.started[0]=0;}
inline const char *stateLabel(State s){switch(s){case State::Idle:return "IDLE";case State::Preparing:return "PREPARING";case State::Printing:return "PRINTING";case State::Paused:return "PAUSED";case State::Finished:return "FINISHED";case State::Failed:return "PRINT FAILED";default:return "WAITING FOR STATUS";}}
inline float fahrenheit(float c){return c*1.8f+32.f;}
inline void durationLabel(int minutes,char *out,size_t size){if(minutes<0)snprintf(out,size,"--");else if(minutes<60)snprintf(out,size,"%d MIN",minutes);else snprintf(out,size,"%dH %02dM",minutes/60,minutes%60);}
// MQTT delivers large reports in fragments. Only a complete, ordered report is parsed.
struct Message {
  char *data=nullptr;size_t capacity=0,length=0,total=0;bool valid=false;
  Message(char *buffer,size_t bytes):data(buffer),capacity(bytes){}
  void reset(){valid=false;length=total=0;}
  bool append(const char *part,int bytes,int offset,int expected,bool correctTopic){
    if(offset==0){reset();if(correctTopic&&expected>0&&size_t(expected)<=MessageLimit&&size_t(expected)<capacity){valid=true;total=size_t(expected);}}
    if(!valid||!part||bytes<0||offset<0||expected<0||size_t(offset)!=length||size_t(expected)!=total||size_t(bytes)>total-length){reset();return false;}
    memcpy(data+length,part,size_t(bytes));length+=size_t(bytes);data[length]=0;return length==total;
  }
};
}
