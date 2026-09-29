#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <algorithm>
namespace printer {
constexpr unsigned PageCount=3;
inline unsigned nextPage(unsigned page,int direction){return (page+PageCount+(direction<0?-1:1))%PageCount;}
constexpr uint32_t CameraIntervalMs=5000,CameraFreshMs=15000,CameraFrameTimeoutMs=10000;
constexpr size_t CameraJpegLimit=1024*1024;
constexpr int CameraWidth=260,CameraHeight=196;
enum class CameraState {Waiting,Capturing,Current,Offline,Clock,Certificate,InvalidFrame,Memory,Config};
struct CameraView {
  const uint16_t *pixels=nullptr;int width=0,height=0;
  uint32_t received=0,revision=0;CameraState state=CameraState::Waiting;
};
inline bool cameraFresh(const CameraView &v,uint32_t now){return v.revision&&(v.state==CameraState::Current||v.state==CameraState::Capturing)&&uint32_t(now-v.received)<=CameraFreshMs;}
struct CameraSchedule {
  bool attempted=false;uint32_t last=0;
  bool due(bool visible,uint32_t now)const{return visible&&(!attempted||uint32_t(now-last)>=CameraIntervalMs);}
  void started(uint32_t now){attempted=true;last=now;}
};
struct CameraRetry {
  bool pending=false;uint32_t since=0,delayMs=0;
  bool due(uint32_t now)const{return !pending||uint32_t(now-since)>=delayMs;}
  void failed(uint32_t now){since=now;delayMs=delayMs?std::min(delayMs*2,uint32_t(60000)):5000;pending=true;}
  void recovered(){pending=false;delayMs=0;}
};
inline void cameraAuth(uint8_t (&packet)[80],const char *code){
  memset(packet,0,sizeof(packet));packet[0]=0x40;packet[5]=0x30;
  memcpy(packet+16,"bblp",4);memcpy(packet+48,code,std::min(size_t(32),strlen(code)));
}
inline uint32_t cameraLe32(const uint8_t *p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline size_t cameraPayloadSize(const uint8_t *header,size_t length){
  if(!header||length!=16)return 0;
  uint32_t size=cameraLe32(header);
  if(size<4||size>CameraJpegLimit||cameraLe32(header+4)!=0||cameraLe32(header+8)!=1||cameraLe32(header+12)!=0)return 0;
  return size;
}
// Assemble one frame without assuming TLS reads align to headers or images.
// The caller swaps completed buffers and immediately resumes draining the stream.
class CameraFrameReader {
 public:
  enum class Result {Pending,Complete,Invalid};
  void reset(uint8_t *buffer,size_t capacity){body=buffer;limit=capacity;headerUsed=bodyUsed=length=0;invalid=false;}
  uint8_t *target(){return length?body+bodyUsed:header+headerUsed;}
  size_t remaining()const{return invalid?0:length?length-bodyUsed:sizeof(header)-headerUsed;}
  size_t size()const{return length;}
  Result advance(size_t count){
    if(invalid||!count||count>remaining()){invalid=true;return Result::Invalid;}
    if(!length){
      headerUsed+=count;
      if(headerUsed==sizeof(header)){length=cameraPayloadSize(header,sizeof(header));if(!body||!length||length>limit){invalid=true;return Result::Invalid;}}
    }else{bodyUsed+=count;if(bodyUsed==length)return Result::Complete;}
    return Result::Pending;
  }
 private:
  uint8_t header[16]={},*body=nullptr;size_t headerUsed=0,bodyUsed=0,length=0,limit=0;bool invalid=false;
};
inline bool cameraJpegValid(const uint8_t *data,size_t length){return data&&length>=4&&length<=CameraJpegLimit&&data[0]==0xff&&data[1]==0xd8&&data[length-2]==0xff&&data[length-1]==0xd9;}
inline bool cameraDimensions(const uint8_t *data,size_t length,int &width,int &height){
  if(!cameraJpegValid(data,length))return false;
  size_t at=2;bool found=false;
  while(at+4<=length){
    if(data[at++]!=0xff)return false;
    while(at<length&&data[at]==0xff)++at;
    if(at+3>length)return false;
    uint8_t marker=data[at++];size_t size=(size_t(data[at])<<8)|data[at+1];
    if(size<2||size>length-at)return false;
    if(marker==0xc0){
      if(found||size<8||data[at+2]!=8)return false;
      int components=data[at+7];if((components!=1&&components!=3)||size!=size_t(8+3*components))return false;
      height=(int(data[at+3])<<8)|data[at+4];width=(int(data[at+5])<<8)|data[at+6];
      // S3's ROM decoder needs whole scaled pixels; P1S frames are 1280 x 720.
      if(width<8||height<8||width>1920||height>1080||width%8||height%8)return false;
      found=true;
    }else if(marker>=0xc1&&marker<=0xcf&&marker!=0xc4&&marker!=0xc8&&marker!=0xcc)return false;
    if(marker==0xda)return found&&at+size<length-2;
    if(marker==0xd8||marker==0xd9||marker==0||marker==0x01||(marker>=0xd0&&marker<=0xd7))return false;
    at+=size;
  }
  return false;
}
// Exact reads work across arbitrary TLS record boundaries, including fragmented headers.
template<class Reader> bool cameraReadExact(Reader read,uint8_t *target,size_t size){
  size_t at=0;while(at<size){int count=read(target+at,size-at);if(count<=0||size_t(count)>size-at)return false;at+=size_t(count);}return true;
}
inline bool cameraFit(int sourceWidth,int sourceHeight,int &width,int &height){
  if(sourceWidth<=0||sourceHeight<=0||sourceWidth>1920||sourceHeight>1080)return false;
  width=CameraWidth;height=sourceHeight*width/sourceWidth;
  if(height>CameraHeight){height=CameraHeight;width=sourceWidth*height/sourceHeight;}
  return width>0&&height>0;
}
inline void cameraResize(const uint16_t *source,int sw,int sh,uint16_t *dest,int dw,int dh){
  // Bilinear RGB565 scaling preserves the full camera view without cropping.
  for(int y=0;y<dh;y++)for(int x=0;x<dw;x++){
    int fx=dw>1?x*(sw-1)*256/(dw-1):0,fy=dh>1?y*(sh-1)*256/(dh-1):0;
    int sx=fx>>8,sy=fy>>8,ax=fx&255,ay=fy&255;
    uint16_t p[]={source[sy*sw+sx],source[sy*sw+std::min(sx+1,sw-1)],source[std::min(sy+1,sh-1)*sw+sx],source[std::min(sy+1,sh-1)*sw+std::min(sx+1,sw-1)]};
    auto channel=[&](int shift,int mask){int a=((p[0]>>shift)&mask)*(256-ax)+((p[1]>>shift)&mask)*ax,b=((p[2]>>shift)&mask)*(256-ax)+((p[3]>>shift)&mask)*ax;return (a*(256-ay)+b*ay+32768)>>16;};
    dest[y*dw+x]=uint16_t((channel(11,31)<<11)|(channel(5,63)<<5)|channel(0,31));
  }
}
}
