#pragma once
#include "sky_draw.h"
#include "sky_trails.h"
namespace sky {
inline void drawTrail(Canvas &c,const Trail &trail,const Snapshot &snapshot,uint32_t now,bool demo=false,bool automatic=false){
  c.clear();const Aircraft &a=trail.aircraft;char text[64];const char *id=callsign(a);
  c.text(19,13,id,c.textWidth(id,fontTitle)>240?fontNumber:fontTitle,White);
  c.text(20,65,automatic?"CIRCLING DETECTED":"15 MIN FLIGHT PATH",fontSmall,White);
  c.right(260,65,demo?"DEMO":snapshot.localSource?"LOCAL":"PUBLIC",fontSmall,Accent);
  TrailBounds bounds=trailBounds(trail);float scale=200.f/bounds.span;
  auto screen=[&](const TrailPoint &p){return Point{140+(p.east-bounds.east)*scale,210-(p.north-bounds.north)*scale};};
  for(int i=0;i<5;i++){float p=30+i*55;c.line(p,100,p,320,Dim,110);c.line(30,100+i*55,250,100+i*55,Dim,110);}
  for(unsigned i=1;i<trail.count;i++){
    const auto &prev=trail.point(i-1),&p=trail.point(i);if(uint32_t(p.at-prev.at)>TrailGap)continue;
    Point from=screen(prev),to=screen(p);int alpha=140+int(115*i/std::max(1u,trail.count-1));
    c.line(from.x,from.y,to.x,to.y,Accent,alpha);c.line(from.x+1,from.y,to.x+1,to.y,Accent,alpha*3/4);c.line(from.x-1,from.y,to.x-1,to.y,Accent,alpha/2);
  }
  TrailPoint observation=trailPosition(a,trail.lastSeen);Point plane=screen(observation);
  if(trail.count){const auto &last=trail.point(trail.count-1);if(uint32_t(trail.lastSeen-last.at)<=TrailGap){Point p=screen(last);c.line(p.x,p.y,plane.x,plane.y,Accent);}}
  Point home=screen(TrailPoint{});bool homeVisible=home.x>=32&&home.x<=248&&home.y>=102&&home.y<=318;
  if(homeVisible){
    c.rect(int(home.x)-5,int(home.y)-5,11,11,0);
    c.line(home.x-6,home.y,home.x,home.y-6,White);c.line(home.x,home.y-6,home.x+6,home.y,White);
    c.line(home.x-4,home.y-1,home.x-4,home.y+6,White);c.line(home.x+4,home.y-1,home.x+4,home.y+6,White);c.line(home.x-4,home.y+6,home.x+4,home.y+6,White);
  }else{
    float dx=home.x-140,dy=home.y-210,n=std::max(fabsf(dx),fabsf(dy));home={140+dx*104/n,210+dy*104/n};
    float length=sqrtf(dx*dx+dy*dy);dx/=length;dy/=length;
    Point arrow[]={{home.x+dx*6,home.y+dy*6},{home.x-dx*4-dy*4,home.y-dy*4+dx*4},{home.x-dx*4+dy*4,home.y-dy*4-dx*4}};c.polygon(arrow,3,White);
  }
  int homeX=std::max(32,std::min(206,int(home.x)+9)),homeY=std::max(103,std::min(291,int(home.y)+8));
  c.text(homeX,homeY,"HOME",fontSmall,White);
  bool fresh=snapshot.status==Status::Live&&uint32_t(now-trail.lastSeen)<=15000;
  if(isfinite(a.track)){
    float angle=a.track*float(Pi)/180,dx=sinf(angle),dy=-cosf(angle);
    Point arrow[]={{plane.x+dx*10,plane.y+dy*10},{plane.x-dx*6-dy*5,plane.y-dy*6+dx*5},{plane.x-dx*3,plane.y-dy*3},{plane.x-dx*6+dy*5,plane.y-dy*6-dx*5}};
    c.polygon(arrow,4,fresh?Accent:White);
  }else{c.circle(plane.x,plane.y,5,fresh?Accent:White);c.circle(plane.x,plane.y,2,fresh?Accent:White);}
  c.rect(238,93,23,29,0);c.text(245,94,"N",fontSmall,White);c.line(248,122,248,114,White);c.line(248,114,245,118,White);c.line(248,114,251,118,White);
  float target=bounds.span*.25f,power=powf(10.f,floorf(log10f(target))),unit=target/power;
  float miles=(unit>=5?5:unit>=2?2:1)*power;float pixels=miles*scale;
  c.rect(23,301,std::max(58,int(pixels)+8),31,0);
  if(miles<1)snprintf(text,sizeof(text),"%.2g MI",double(miles));else snprintf(text,sizeof(text),"%.0f MI",double(miles));
  c.text(26,303,text,fontSmall,White);c.line(26,326,26+pixels,326,White);c.line(26,323,26,329,White);c.line(26+pixels,323,26+pixels,329,White);
  c.text(20,347,"ALT / FT",fontSmall,White);c.text(112,347,"SPEED / KT",fontSmall,White);c.right(260,347,"MI",fontSmall,White);
  if(isfinite(a.altitude))snprintf(text,sizeof(text),"%d",int(roundf(a.altitude)));else strcpy(text,"--");c.text(20,374,text,fontHeading,White,0,108);
  if(isfinite(a.speed))snprintf(text,sizeof(text),"%d",int(roundf(a.speed)));else strcpy(text,"--");c.text(112,374,text,fontHeading,White,0,198);
  snprintf(text,sizeof(text),"%.1f",double(a.distance));c.right(260,374,text,fontHeading,Accent);
  uint32_t age=uint32_t(now-trail.lastSeen)/1000;
  if(age<60)snprintf(text,sizeof(text),"%s / %u SEC AGO",fresh?"LIVE":"LAST SEEN",unsigned(age));
  else snprintf(text,sizeof(text),"LAST SEEN / %u MIN AGO",unsigned(age/60));
  if(trail.count<2&&age<15)strcpy(text,"BUILDING FLIGHT PATH");
  c.text((Width-c.textWidth(text,fontSmall))/2,407,text,fontSmall,White);
  const char *hint="TAP TO EXIT TRACKING";c.text((Width-c.textWidth(hint,fontSmall))/2,427,hint,fontSmall,White);
}
}
