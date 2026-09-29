#pragma once
#include "space_data.h"
#include "../../core/canvas.h"

namespace spacewx {
static const uint16_t Ink=ui::rgb(255,255,255),Accent=ui::rgb(91,246,207),Grid=ui::rgb(25,63,68),Amber=ui::rgb(255,202,93),Red=ui::rgb(255,105,100);
inline uint16_t alertColor(int level){return level>=3?Red:level>0?Amber:Accent;}
inline void centered(ui::Canvas &c,int x,int y,const char *s,const ui::Font &font,uint16_t color){c.text(x-c.textWidth(s,font)/2,y,s,font,color,0,280);}
inline void ellipse(ui::Canvas &c,float cx,float cy,float rx,float ry,float tilt,uint16_t color,int alpha=255){
  float cs=cosf(tilt),sn=sinf(tilt),px=0,py=0;
  for(int i=0;i<=96;i++){float a=i*2*ui::Pi/96,x=rx*cosf(a),y=ry*sinf(a),xx=cx+x*cs-y*sn,yy=cy+x*sn+y*cs;if(i)c.line(px,py,xx,yy,color,alpha);px=xx;py=yy;}
}
inline void globe(ui::Canvas &c,const Snapshot &v,uint32_t now,int64_t epoch){
  constexpr float cx=140,cy=172,radius=57;
  bool current=kpFresh(v,epoch);float kp=current?v.kp.value:0;
  // Decorative activity illustration: the oval responds to Kp, not a local
  // aurora probability map. Missing data freezes the particles and dims it.
  float phase=current?(now%240000)/1000.f:0;
  float flow=windFresh(v,epoch)?std::min(1.3f,std::max(.45f,v.wind.mph/1800000.f)):.65f;
  uint16_t color=alertColor(activity(v,epoch));
  static const ui::Point stars[]={{27,110},{241,94},{59,225},{257,213},{26,191},{209,114},{76,104},{219,235},{54,151}};
  for(auto p:stars)c.rect(int(p.x),int(p.y),1,1,ui::rgb(135,169,191));
  ellipse(c,cx,cy,107,46,-.24f,Grid,170);ellipse(c,cx,cy,86,65,-.24f,Grid,160);
  for(int i=0;i<7;i++){
    float a=phase*flow*.24f+i*2*ui::Pi/7,x=107*cosf(a),y=46*sinf(a);
    float xx=cx+x*.9713f+y*.2377f,yy=cy-x*.2377f+y*.9713f;
    c.circle(xx,yy,1.5f,current?color:Grid,190);
  }
  for(int y=-57;y<=57;y++)for(int x=-57;x<=57;x++){
    float rr=(x*x+y*y)/(radius*radius);if(rr>1)continue;
    float z=sqrtf(1-rr),light=std::max(.12f,.72f*z-.34f*x/radius-.26f*y/radius);
    c.pixel(int(cx)+x,int(cy)+y,ui::rgb(int(5+9*light),int(15+43*light),int(27+58*light)));
  }
  for(int i=-2;i<=2;i++){
    float y=i*radius/3,rx=sqrtf(radius*radius-y*y);ellipse(c,cx,cy+y,rx,rx*.12f,0,Grid,130);
  }
  for(int i=1;i<=3;i++)ellipse(c,cx,cy,radius*i/4,radius,0,Grid,145);
  // Stylized continental shapes are intentionally not a geographic forecast.
  const ui::Point north[]={{103,135},{117,124},{139,126},{148,137},{140,144},{134,155},{121,161},{124,171},{113,166},{106,151},{99,147}};
  const ui::Point south[]={{127,172},{141,169},{152,184},{146,201},{139,209},{135,217},{130,203},{131,191},{123,183}};
  const ui::Point east[]={{168,139},{179,145},{183,155},{173,159},{170,168},{178,182},{171,198},{161,198},{156,182},{163,170},{157,158}};
  c.polygon(north,11,ui::rgb(24,74,81));c.polygon(south,9,ui::rgb(24,74,81));c.polygon(east,11,ui::rgb(24,74,81));
  c.circle(cx,cy,radius,ui::rgb(58,134,163));c.circle(cx,cy,radius+2,Accent,45);
  // Broad auroral ribbons grow and ripple as geomagnetic activity rises.
  for(int band=0;band<7;band++){
    float px=0,py=0;
    for(int i=0;i<=60;i++){
      float t=i/60.f,x=-43+86*t;
      float y=-28+12*cosf((t-.5f)*ui::Pi)+kp*.8f+band*1.45f+sinf(t*15+phase*.7f+band*.2f)*(1.2f+kp*.35f);
      float xx=cx+x,yy=cy+y;
      uint16_t ribbon=band<4?Accent:ui::rgb(159,124,255);
      if(i)c.line(px,py,xx,yy,current?ribbon:Grid,current?int(90+band*16):100);
      if(i&&band==0)c.line(xx,yy,xx,yy-4-kp*.6f,Accent,current?70:0);
      px=xx;py=yy;
    }
  }
}
inline void scaleBadge(ui::Canvas &c,int cx,int y,const char *label,char code,int value,bool fresh){
  centered(c,cx,y,label,ui::fontSmall,Ink);char level[8];snprintf(level,sizeof(level),value>=0&&fresh?"%c%d":"%c--",code,value);
  centered(c,cx,y+24,level,ui::fontNumber,fresh?alertColor(value):Ink);
  for(int i=0;i<5;i++)c.rect(cx-27+i*11,y+58,8,3,fresh&&value>i?alertColor(value):Grid);
}
inline void sourceFooter(ui::Canvas &c,const Snapshot &v,unsigned page,int64_t epoch,bool demo){
  bool s=scalesFresh(v,epoch),k=kpFresh(v,epoch),w=windFresh(v,epoch);
  const char *state=demo?"DEMO":s&&k&&w&&complete(v.scales)?"LIVE":s||k||w?"PARTIAL":v.scales.stamp||v.kp.stamp||v.wind.stamp?"STALE":"WAITING";
  c.text(20,402,"NOAA / SWPC",ui::fontSmall,Ink);c.right(260,402,state,ui::fontSmall,strcmp(state,"LIVE")?Amber:Accent);
  char label[48];snprintf(label,sizeof(label),"%u/3  SWIPE UP/DOWN",page+1);centered(c,140,426,label,ui::fontSmall,Ink);
}
inline void overview(ui::Canvas &c,const Snapshot &v,uint32_t now,int64_t epoch){
  bool s=scalesFresh(v,epoch),k=kpFresh(v,epoch),w=windFresh(v,epoch);
  const char *title=headline(v,epoch);uint16_t color=alertColor(activity(v,epoch));
  if(!strcmp(title,"PARTIAL DATA")||!strcmp(title,"DATA DELAYED")||!strcmp(title,"WI-FI OFFLINE"))color=Amber;
  c.text(20,43,title,ui::fontHeading,color);c.text(20,74,summary(v,epoch),ui::fontSmall,Ink);
  globe(c,v,now,epoch);
  c.text(20,245,"EST. KP",ui::fontSmall,Ink);c.text(149,245,"WIND / MPH",ui::fontSmall,Ink);
  char value[32];if(k)snprintf(value,sizeof(value),"%.1f",double(v.kp.value));else strcpy(value,"--");
  c.text(19,271,value,ui::fontTitle,k?alertColor(activity(v,epoch)):Ink);
  speedText(w?v.wind.mph:NAN,value,sizeof(value));c.text(148,275,value,ui::fontNumber,w?Accent:Ink);
  scaleBadge(c,53,326,"GEOMAG",'G',v.scales.g,s);scaleBadge(c,140,326,"SOLAR",'S',v.scales.s,s);scaleBadge(c,227,326,"RADIO",'R',v.scales.r,s);
}
inline void scalesPage(ui::Canvas &c,const Snapshot &v,int64_t epoch){
  bool fresh=scalesFresh(v,epoch);char value[64];
  c.text(20,47,"CURRENT NOAA SCALES",ui::fontSmall,Ink);
  const char *labels[]={"GEOMAGNETIC","SOLAR RADIATION","RADIO BLACKOUT"};
  const char *notes[]={"G / MAGNETIC FIELD","S / SOLAR PARTICLES","R / HF RADIO"};
  const char codes[]={'G','S','R'};int values[]={v.scales.g,v.scales.s,v.scales.r};
  for(int i=0;i<3;i++){
    int y=78+i*77,level=fresh?values[i]:-1;uint16_t color=level>=0?alertColor(level):Ink;
    c.text(20,y,labels[i],ui::fontSmall,Ink);
    if(level>=0)snprintf(value,sizeof(value),"%c%d",codes[i],level);else snprintf(value,sizeof(value),"%c--",codes[i]);
    c.text(19,y+27,value,ui::fontNumber,color);c.text(99,y+30,levelName(level),ui::fontHeading,color);
    c.text(99,y+50,notes[i],ui::fontSmall,Ink);
  }
  c.text(20,323,"G OUTLOOK / UTC DAYS",ui::fontSmall,Ink);
  const char *days[]={"TODAY","TOMORROW","DAY 3"};
  for(int i=0;i<3;i++){
    const auto &day=v.scales.outlook[i];int level=fresh&&day.day==epoch/86400+i?day.g:-1;
    int cx=53+i*87;centered(c,cx,347,days[i],ui::fontSmall,Ink);
    if(level>=0)snprintf(value,sizeof(value),"G%d",level);else strcpy(value,"G--");
    centered(c,cx,371,value,ui::fontHeading,level>=0?alertColor(level):Ink);
  }
}
inline void trendPage(ui::Canvas &c,const Snapshot &v,int64_t epoch){
  bool current=kpFresh(v,epoch);char value[64];
  c.text(20,47,"ESTIMATED KP / PAST 6H",ui::fontSmall,Ink);
  if(current)snprintf(value,sizeof(value),"%.1f",double(v.kp.value));else strcpy(value,"--");
  c.text(19,79,value,ui::fontTitle,current?alertColor(activity(v,epoch)):Ink);
  ageText(v.kp.stamp,epoch,value,sizeof(value));c.right(260,93,value,ui::fontSmall,Ink);
  constexpr int left=27,right=257,top=152,bottom=282;
  auto y=[](float kp){return bottom-kp*(bottom-top)/9;};
  for(int level:{0,3,5,9}){
    snprintf(value,sizeof(value),"%d",level);c.text(9,int(y(level))-8,value,ui::fontSmall,Ink);
    c.line(left,y(level),right,y(level),level==5?Amber:Grid,level==5?130:160);
  }
  int64_t end=v.kp.stamp,start=end-HistorySeconds;ui::Point previous={};int64_t previousStamp=0;
  for(int i=0;i<v.kp.count;i++){
    const auto &sample=v.kp.samples[i];if(sample.stamp<start||sample.stamp>end||!isfinite(sample.value))continue;
    ui::Point p={left+float(sample.stamp-start)/HistorySeconds*(right-left),y(sample.value)};
    // A missing interval remains a visible break, never a fabricated straight line.
    if(previousStamp&&sample.stamp-previousStamp<=10*60)c.line(previous.x,previous.y,p.x,p.y,current?Accent:Grid);
    c.circle(p.x,p.y,1,current?(sample.value>=5?Amber:Accent):Grid);previous=p;previousStamp=sample.stamp;
  }
  if(!v.kp.count)centered(c,142,207,"WAITING FOR HISTORY",ui::fontSmall,Ink);
  c.text(27,289,"-6H",ui::fontSmall,Ink);centered(c,142,289,"-3H",ui::fontSmall,Ink);c.right(260,289,current?"NOW":"LAST",ui::fontSmall,Ink);
  c.text(20,323,"KP 5+ / STORM THRESHOLD",ui::fontSmall,Amber);
  c.text(20,353,"GLOBAL ACTIVITY INDEX",ui::fontSmall,Ink);
  c.text(20,377,"ILLUSTRATED AURORA",ui::fontSmall,Ink);
}
inline void render(ui::Canvas &c,const Snapshot &v,unsigned page,uint32_t now,int64_t epoch,bool demo=false){
  c.clear();page%=3;c.text(20,12,"SPACE WEATHER",ui::fontBody,Ink);c.right(260,14,page==0?"EARTH":page==1?"SCALES":"TREND",ui::fontSmall,Accent);
  if(page==0)overview(c,v,now,epoch);else if(page==1)scalesPage(c,v,epoch);else trendPage(c,v,epoch);
  sourceFooter(c,v,page,epoch,demo);
}
}
