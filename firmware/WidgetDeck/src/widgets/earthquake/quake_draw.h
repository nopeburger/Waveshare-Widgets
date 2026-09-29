#pragma once
#include "quake_data.h"
#include "../../core/canvas.h"

namespace quake {
static const uint16_t Ink=ui::rgb(255,255,255),Accent=ui::rgb(255,224,64),Muted=ui::rgb(154,147,139),Grid=ui::rgb(55,49,20);
inline void placeLines(ui::Canvas &c,const char *place,int top=158){
  const char *cursor=place;
  for(int row=0;row<2&&*cursor;row++){
    while(*cursor==' ')++cursor;
    char text[112]={};size_t n=0,word=0;
    while(cursor[n]&&n+1<sizeof(text)){
      text[n]=cursor[n];text[n+1]=0;
      if(c.textWidth(text,ui::fontBody)>240){text[n]=0;break;}
      if(cursor[n]==' ')word=n;++n;
    }
    if(cursor[n]&&row==0&&word){
      n=word;
      // Keep a short state/country suffix with the preceding place name.
      if(strlen(cursor+word+1)<5){size_t prior=word;while(prior&&cursor[prior-1]!=' ')--prior;if(prior)n=prior-1;}
      text[n]=0;
    }
    if(cursor[n]&&row==1){
      while(n&&c.textWidth(text,ui::fontBody)+c.textWidth("...",ui::fontBody)>240)text[--n]=0;
      strncat(text,"...",sizeof(text)-strlen(text)-1);
    }
    c.text(20,top+row*24,text,ui::fontBody,Ink);cursor+=n;
  }
}
inline void epicenters(ui::Canvas &c,const Slideshow &show){
  constexpr float cx=140,cy=175,radius=86;
  c.circle(cx,cy,radius,Grid);c.circle(cx,cy,radius/2,Grid,150);
  c.line(cx-radius,cy,cx+radius,cy,Grid,100);c.line(cx,cy-radius,cx,cy+radius,Grid,100);
  c.text(int(cx)-5,int(cy-radius)-19,"N",ui::fontSmall,Ink);
  auto point=[](const Event &e){float a=e.bearing*ui::Pi/180,r=std::min(e.distance/RadiusMiles,1.f)*radius;return ui::Point{cx+sinf(a)*r,cy-cosf(a)*r};};
  for(int i=0;i<show.count;i++){auto p=point(show.events[i]);c.circle(p.x,p.y,1.8f,Muted,180);}
  c.rect(int(cx)-3,int(cy)-3,6,6,Ink);
  if(const Event *e=show.current()){auto p=point(*e);c.line(cx,cy,p.x,p.y,Accent,170);for(int r=0;r<=4;r++)c.circle(p.x,p.y,float(r),Accent);c.circle(p.x,p.y,8,Accent,170);}
}
inline void render(ui::Canvas &c,const Slideshow &show,const Snapshot &view,uint32_t now,int64_t epoch){
  c.clear();
  const Event *event=show.current();
  if(!event){
    c.text(20,16,"EARTHQUAKES",ui::fontBody,Accent);c.right(260,20,"100 MI",ui::fontSmall,Ink);c.rect(20,47,240,1,Grid);
    c.circle(140,149,53,Grid);c.circle(140,149,28,Grid);c.rect(138,147,4,4,Accent);
    const char *title="LOOKING UP QUAKES",*detail="CONNECTING TO USGS";
    if(view.status==Status::Live&&fresh(view,epoch)){title="NO RECENT QUAKES";detail="REPORTED WITHIN 100 MI";}
    else if(view.status==Status::Live){title="WAITING FOR UPDATE";detail="NO CACHED REPORTS";}
    else if(view.status==Status::Offline){title="WI-FI OFFLINE";detail="WAITING FOR CONNECTION";}
    else if(view.status==Status::Clock){title="SYNCING TIME";detail="WAITING FOR NETWORK TIME";}
    else if(view.status==Status::Error){title="FEED UNAVAILABLE";detail="RETRYING AUTOMATICALLY";}
    c.text(20,234,title,ui::fontHeading,Ink);c.text(20,277,detail,ui::fontSmall,Ink);
    c.text(20,333,"PAST 7 DAYS",ui::fontBody,Accent);c.text(20,365,"USGS EARTHQUAKE CATALOG",ui::fontSmall,Ink);
    c.text(20,425,"HOLD FOR WI-FI SETTINGS",ui::fontSmall,Ink);return;
  }
  char label[80];if(isfinite(event->magnitude))snprintf(label,sizeof(label),"M %.1f",double(event->magnitude));else strcpy(label,"M --");
  c.text(19,10,label,ui::fontTitle,Ink);c.right(260,15,"QUAKES",ui::fontSmall,Accent);c.right(260,37,"100 MI",ui::fontBody,Ink);
  ageLabel(event->occurred,epoch,label,sizeof(label));const ui::Font &ageFont=c.textWidth(label,ui::fontHeading)>108?ui::fontBody:ui::fontHeading;c.text(20,62,label,ageFont,Accent,0,130);
  c.right(260,67,event->reviewed?"REVIEWED":"PRELIMINARY",ui::fontSmall,Ink);
  epicenters(c,show);placeLines(c,event->place,276);
  c.text(20,332,"FROM YOU / MI",ui::fontSmall,Ink);c.text(169,332,"DEPTH / MI",ui::fontSmall,Ink);
  snprintf(label,sizeof(label),"%.1f",double(event->distance));c.text(19,359,label,ui::fontNumber,Ink,0,124);c.text(125,368,sky::compass(event->bearing),ui::fontSmall,Accent);
  if(isfinite(event->depthKm))snprintf(label,sizeof(label),"%.1f",milesFromKm(event->depthKm));else strcpy(label,"--");
  const ui::Font &depthFont=c.textWidth(label,ui::fontNumber)>92?ui::fontHeading:ui::fontNumber;c.text(168,359,label,depthFont,Ink,0,265);
  c.rect(20,390,240,2,Grid);c.rect(20,390,int(240*show.progress(now)),2,Accent);
  snprintf(label,sizeof(label),"%d / %d%s",show.index+1,show.count,show.held?"  HELD":"");c.text(20,397,label,ui::fontBody,Ink);c.right(260,397,fresh(view,epoch)?"LIVE":"CACHED",ui::fontBody,Accent);
  c.text(20,427,"USGS",ui::fontSmall,Ink);dateLabel(event->occurred,label,sizeof(label));c.right(260,427,label,ui::fontSmall,Ink);
}
}
