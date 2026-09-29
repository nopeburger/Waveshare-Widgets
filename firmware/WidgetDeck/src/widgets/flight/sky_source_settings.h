#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
namespace sky {
enum class Source : uint8_t { Public, MeshPoint };
struct SourceSettings {
  Source source=Source::Public;
  char ip[16]={},password[513]={};
  uint16_t dashboardPort=8080,receiverPort=8081;
};
inline bool receiverAddress(const char *s){
  unsigned parts[4]={};const char *p=s;
  for(int i=0;i<4;i++){
    if(!isdigit((unsigned char)*p))return false;
    const char *start=p;char *end;unsigned long n=strtoul(p,&end,10);
    if(n>255||end-start>3||(end-start>1&&*start=='0'))return false;
    parts[i]=unsigned(n);p=end;if(i<3){if(*p++!='.')return false;}else if(*p)return false;
  }
  return parts[0]==10||(parts[0]==172&&parts[1]>=16&&parts[1]<=31)||(parts[0]==192&&parts[1]==168);
}
inline bool validSourceSettings(const SourceSettings &s){
  if(s.source==Source::Public)return true;
  return s.source==Source::MeshPoint&&receiverAddress(s.ip)&&s.dashboardPort&&s.receiverPort&&s.password[0]&&strcmp(s.password,"YOUR_VIEWER_PASSWORD");
}
// Literal INI values: preserve spaces, #, = and UTF-8 in the password.
inline bool parseSourceSettings(const char *data,size_t size,SourceSettings &out){
  if(!data||!size||size>2048||memchr(data,0,size))return false;
  SourceSettings next;unsigned seen=0;size_t pos=0;
  if(size>=3&&(unsigned char)data[0]==0xef&&(unsigned char)data[1]==0xbb&&(unsigned char)data[2]==0xbf)pos=3;
  while(pos<size){
    size_t start=pos;while(pos<size&&data[pos]!='\n')pos++;size_t end=pos;if(pos<size)pos++;
    if(end>start&&data[end-1]=='\r')end--;
    while(start<end&&(data[start]==' '||data[start]=='\t'))start++;
    if(start==end||data[start]=='#'||data[start]==';')continue;
    size_t eq=start;while(eq<end&&data[eq]!='=')eq++;if(eq==end)return false;
    size_t keyEnd=eq;while(keyEnd>start&&(data[keyEnd-1]==' '||data[keyEnd-1]=='\t'))keyEnd--;
    char key[24]={};if(keyEnd-start>=sizeof(key))return false;memcpy(key,data+start,keyEnd-start);
    size_t len=end-eq-1;const char *value=data+eq+1;unsigned bit=0;
    if(!strcmp(key,"source")){
      bit=1;
      if(len==6&&!memcmp(value,"public",6))next.source=Source::Public;
      else if(len==9&&!memcmp(value,"meshpoint",9))next.source=Source::MeshPoint;
      else return false;
    }else if(!strcmp(key,"ip")||!strcmp(key,"viewer_password")){
      bool ip=!strcmp(key,"ip");bit=ip?2:4;char *dest=ip?next.ip:next.password;size_t cap=ip?sizeof(next.ip):sizeof(next.password);
      if(!len||len>=cap)return false;
      for(size_t i=0;i<len;i++)if((unsigned char)value[i]<32||value[i]==127)return false;
      memcpy(dest,value,len);dest[len]=0;
    }else if(!strcmp(key,"dashboard_port")||!strcmp(key,"receiver_port")){
      bool dashboard=!strcmp(key,"dashboard_port");bit=dashboard?8:16;
      if(!len||len>5)return false;unsigned port=0;
      for(size_t i=0;i<len;i++){if(value[i]<'0'||value[i]>'9')return false;port=port*10+unsigned(value[i]-'0');}
      if(!port||port>65535)return false;
      if(dashboard)next.dashboardPort=uint16_t(port);else next.receiverPort=uint16_t(port);
    }else return false;
    if(seen&bit)return false;seen|=bit;
  }
  if(!(seen&1)||!validSourceSettings(next))return false;
  if(next.source==Source::Public)next=SourceSettings();
  out=next;return true;
}
inline uint64_t sourceFingerprint(const char *data,size_t size){
  uint64_t hash=14695981039346656037ULL;
  for(size_t i=0;i<size;i++){hash^=static_cast<unsigned char>(data[i]);hash*=1099511628211ULL;}return hash;
}
}
