#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
namespace printer {
struct Settings {char ip[16]={},serial[33]={},accessCode[33]={};bool enabled=true;};
inline bool localAddress(const char *s){
  unsigned parts[4]={};const char *p=s;
  for(int i=0;i<4;i++){if(!isdigit((unsigned char)*p))return false;const char *start=p;char *end;unsigned long n=strtoul(p,&end,10);if(n>255||end-start>3||(end-start>1&&*start=='0'))return false;parts[i]=unsigned(n);p=end;if(i<3){if(*p++!='.')return false;}else if(*p)return false;}
  return parts[0]==10||(parts[0]==172&&parts[1]>=16&&parts[1]<=31)||(parts[0]==192&&parts[1]==168);
}
inline bool validSettings(const Settings &s){
  if(!s.enabled)return true;
  if(!localAddress(s.ip)||strlen(s.serial)<6||strlen(s.serial)>32||strlen(s.accessCode)!=8)return false;
  for(const char *p=s.serial;*p;p++)if(!isalnum((unsigned char)*p))return false;
  for(const char *p=s.accessCode;*p;p++)if(!isalnum((unsigned char)*p))return false;
  return strcmp(s.serial,"YOUR_PRINTER_SERIAL")&&strcmp(s.accessCode,"YOURCODE");
}
inline bool parseSettings(const char *data,size_t size,Settings &out){
  if(!data||!size||size>2048||memchr(data,0,size))return false;
  Settings next;unsigned seen=0;size_t pos=0;
  if(size>=3&&(unsigned char)data[0]==0xef&&(unsigned char)data[1]==0xbb&&(unsigned char)data[2]==0xbf)pos=3;
  while(pos<size){size_t start=pos;while(pos<size&&data[pos]!='\n')pos++;size_t end=pos;if(pos<size)pos++;if(end>start&&data[end-1]=='\r')end--;
    while(start<end&&(data[start]==' '||data[start]=='\t'))start++;
    if(start==end||data[start]=='#'||data[start]==';')continue;
    size_t eq=start;while(eq<end&&data[eq]!='=')eq++;if(eq==end)return false;
    size_t keyEnd=eq;while(keyEnd>start&&(data[keyEnd-1]==' '||data[keyEnd-1]=='\t'))keyEnd--;
    char key[24]={};if(keyEnd-start>=sizeof(key))return false;memcpy(key,data+start,keyEnd-start);
    size_t len=end-eq-1;const char *value=data+eq+1;char *dest=nullptr;size_t cap=0;unsigned bit=0;
    if(!strcmp(key,"ip")){dest=next.ip;cap=sizeof(next.ip);bit=1;}
    else if(!strcmp(key,"serial")){dest=next.serial;cap=sizeof(next.serial);bit=2;}
    else if(!strcmp(key,"access_code")){dest=next.accessCode;cap=sizeof(next.accessCode);bit=4;}
    else if(!strcmp(key,"enabled")){bit=8;if(len==4&&!memcmp(value,"true",4))next.enabled=true;else if(len==5&&!memcmp(value,"false",5))next.enabled=false;else return false;}
    else return false;
    if(seen&bit)return false;seen|=bit;
    if(dest){if(!len||len>=cap)return false;memcpy(dest,value,len);dest[len]=0;}
  }
  if(!seen||!validSettings(next))return false;out=next;return true;
}
}
