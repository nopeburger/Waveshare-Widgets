#pragma once
#include "fire_data.h"
#include "../../core/canvas.h"
namespace wildfire {
static const uint16_t Ink=ui::rgb(255,255,255),Accent=ui::rgb(255,188,105),Muted=ui::rgb(150,146,140),Grid=ui::rgb(55,43,29);
inline void fitted(ui::Canvas &c,int x,int y,const char *value,const ui::Font &font,uint16_t color,int width){
  char label[96];snprintf(label,sizeof(label),"%s",value);size_t n=strlen(label);
  if(c.textWidth(label,font)>width){while(n&&c.textWidth(label,font)+c.textWidth("...",font)>width)label[--n]=0;strncat(label,"...",sizeof(label)-strlen(label)-1);}
  c.text(x,y,label,font,color,0,x+width);
}
inline void map(ui::Canvas &c,const Slideshow &show){
  constexpr float cx=140,cy=195,radius=86;
  c.circle(cx,cy,radius,Grid);c.circle(cx,cy,radius/2,Grid,150);c.line(cx-radius,cy,cx+radius,cy,Grid,100);c.line(cx,cy-radius,cx,cy+radius,Grid,100);
  c.text(int(cx)-5,int(cy-radius)-19,"N",ui::fontSmall,Ink);
  auto point=[](const Report &r){float a=r.bearing*ui::Pi/180,d=std::min(r.distance/RadiusMiles,1.f)*radius;return ui::Point{cx+sinf(a)*d,cy-cosf(a)*d};};
  for(int i=0;i<show.count;i++){auto p=point(show.reports[i]);c.circle(p.x,p.y,2.5f,Muted,200);}
  c.rect(int(cx)-3,int(cy)-3,6,6,Ink);
  if(const Report *r=show.current()){auto p=point(*r);c.line(cx,cy,p.x,p.y,Accent,180);for(int s=0;s<=4;s++)c.circle(p.x,p.y,float(s),Accent);c.circle(p.x,p.y,8,Accent,200);}
}
inline void render(ui::Canvas &c,const Slideshow &show,const Snapshot &view,uint32_t now,int64_t epoch){
  c.clear();c.text(20,12,"WILDFIRES",ui::fontBody,Accent);c.right(260,16,"100 MI",ui::fontSmall,Ink);
  const Report *report=show.current();
  if(!report){
    c.rect(20,47,240,1,Grid);
    const ui::Point flame[]={{140,99},{161,126},{159,144},{174,135},{181,166},{168,189},{142,200},{115,189},{100,167},{106,139},{121,150},{119,129}};
    c.polygon(flame,12,Accent);const ui::Point core[]={{140,148},{156,177},{142,191},{126,181},{125,170}};c.polygon(core,5,ui::rgb(85,46,15));
    const char *title="LOOKING UP FIRES",*detail="CONNECTING TO NIFC";
    if(fresh(view,epoch)){title="NO NEARBY REPORTS";detail="CALIFORNIA / WITHIN 100 MI";}
    else if(view.status==Status::Offline){title="WI-FI OFFLINE";detail="WAITING FOR CONNECTION";}
    else if(view.status==Status::Clock){title="SYNCING TIME";detail="WAITING FOR NETWORK TIME";}
    else if(view.status==Status::Error){title="FEED UNAVAILABLE";detail="RETRYING AUTOMATICALLY";}
    else if(view.received){title="WAITING FOR UPDATE";detail="CACHED REPORTS EXPIRED";}
    fitted(c,20,235,title,ui::fontHeading,Ink,240);c.text(20,278,detail,ui::fontSmall,Ink);
    c.text(20,333,"REPORTED FIRE LOCATIONS",ui::fontSmall,Ink);c.text(20,365,"NIFC / CALIFORNIA",ui::fontBody,Accent);c.text(20,425,"HOLD FOR WI-FI SETTINGS",ui::fontSmall,Ink);return;
  }
  const ui::Font &nameFont=c.textWidth(report->name,ui::fontNumber)<=240?ui::fontNumber:ui::fontHeading;fitted(c,19,43,report->name,nameFont,Ink,241);
  fitted(c,20,76,report->county,ui::fontSmall,Ink,240);map(c,show);
  c.text(20,288,"ACRES",ui::fontSmall,Ink);c.text(158,288,"CONTAINED",ui::fontSmall,Ink);
  char value[80];acresLabel(report->acres,value,sizeof(value));const ui::Font &areaFont=c.textWidth(value,ui::fontNumber)>120?ui::fontHeading:ui::fontNumber;c.text(19,315,value,areaFont,Ink,0,141);
  if(isfinite(report->contained))snprintf(value,sizeof(value),"%.0f%%",double(report->contained));else strcpy(value,"--");c.text(157,315,value,ui::fontNumber,Accent,0,265);
  c.text(20,354,"FROM YOU",ui::fontSmall,Ink);snprintf(value,sizeof(value),"%.1f MI %s",double(report->distance),sky::compass(report->bearing));
  const ui::Font &distanceFont=c.textWidth(value,ui::fontHeading)>108?ui::fontBody:ui::fontHeading;
  c.text(157,352,value,distanceFont,Ink,0,265);
  c.rect(20,390,240,2,Grid);c.rect(20,390,int(240*show.progress(now)),2,Accent);
  snprintf(value,sizeof(value),"%d / %d%s",show.index+1,show.count,show.held?"  HELD":"");c.text(20,397,value,ui::fontBody,Ink);c.right(260,397,fresh(view,epoch)?"CURRENT":"CACHED",ui::fontBody,Accent);
  c.text(20,427,"NIFC",ui::fontSmall,Ink);updatedLabel(report->updated,epoch,value,sizeof(value));c.right(260,427,value,ui::fontSmall,Ink);
}
}
