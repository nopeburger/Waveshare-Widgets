#pragma once
#include "printer_camera_data.h"
#include "printer_draw.h"
namespace printer {
inline void renderCamera(ui::Canvas &c,const Snapshot &s,const CameraView &camera,uint32_t now){
  c.clear();c.text(20,12,"BAMBU LAB",ui::fontBody,Accent);c.right(260,16,"P1S",ui::fontSmall,Ink);
  c.text(20,49,"LIVE VIEW",ui::fontHeading,Accent);
  bool image=camera.pixels&&camera.revision&&camera.width>0&&camera.width<=CameraWidth&&camera.height>0&&camera.height<=CameraHeight;
  if(image){
    int left=(ui::Width-camera.width)/2,top=104+(CameraHeight-camera.height)/2;
    for(int y=0;y<camera.height;y++)for(int x=0;x<camera.width;x++)c.pixel(left+x,top+y,camera.pixels[y*camera.width+x]);
  }else{
    // A camera outline, rather than a simulated printer image, while waiting.
    c.rect(103,164,74,2,Accent);c.rect(103,164,2,52,Accent);c.rect(175,164,2,52,Accent);c.rect(103,214,74,2,Accent);
    c.circle(140,190,16,Accent);c.rect(114,157,19,7,Accent);
    const char *label="WAITING FOR IMAGE";
    switch(camera.state){case CameraState::Capturing:label="GETTING SNAPSHOT";break;case CameraState::Offline:label="CAMERA UNAVAILABLE";break;case CameraState::Clock:label="SYNCING TIME";break;case CameraState::Certificate:label="CHECK PRINTER ID";break;case CameraState::InvalidFrame:label="IMAGE UNAVAILABLE";break;case CameraState::Memory:label="CAMERA MEMORY LOW";break;case CameraState::Config:label="CHECK PRINTER SETUP";break;default:break;}
    center(c,250,label,ui::fontBody,Ink);
  }
  char value[64];
  if(image){uint32_t age=uint32_t(now-camera.received)/1000;if(age<60)snprintf(value,sizeof(value),"%u SEC AGO",unsigned(age));else if(age<3600)snprintf(value,sizeof(value),"%u MIN AGO",unsigned(age/60));else snprintf(value,sizeof(value),"%u HR AGO",unsigned(age/3600));c.text(20,304,value,ui::fontBody,Ink);}
  else center(c,304,"RETRIES AUTOMATICALLY",ui::fontSmall,Ink);
  c.text(20,342,"PROGRESS",ui::fontSmall,Ink);c.text(158,342,"TIME LEFT",ui::fontSmall,Ink);
  bool live=fresh(s,now);
  if(live&&s.progress>=0)snprintf(value,sizeof(value),"%d%%",s.progress);else strcpy(value,"--");c.text(19,365,value,ui::fontHeading,Ink);
  durationLabel(live?s.remaining:-1,value,sizeof(value));fitted(c,158,365,value,ui::fontHeading,Ink,102);
  c.rect(20,399,240,1,Grid);c.text(20,410,"3 / 3",ui::fontBody,Ink);
  const char *label=camera.state==CameraState::Capturing?"UPDATING":cameraFresh(camera,now)?"SNAPSHOT":image?"DELAYED":"WAITING";
  c.right(260,410,label,ui::fontBody,cameraFresh(camera,now)?Accent:Ink);
}
}
