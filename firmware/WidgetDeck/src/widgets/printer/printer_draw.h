#pragma once
#include "printer_data.h"
#include "printer_camera_data.h"
#include "../../core/canvas.h"
#include "../../core/numerals.h"
#include <time.h>
namespace printer {
static const uint16_t Ink=ui::rgb(255,255,255),Accent=ui::rgb(218,174,255),Grid=ui::rgb(47,35,59);
inline void fitted(ui::Canvas &c,int x,int y,const char *value,const ui::Font &font,uint16_t color,int width=240){
  char text[120];snprintf(text,sizeof(text),"%s",value);size_t n=strlen(text);
  if(c.textWidth(text,font)>width){while(n&&c.textWidth(text,font)+c.textWidth("...",font)>width)text[--n]=0;strncat(text,"...",sizeof(text)-strlen(text)-1);}
  c.text(x,y,text,font,color,0,x+width);
}
inline void center(ui::Canvas &c,int y,const char *text,const ui::Font &font,uint16_t color){c.text((ui::Width-c.textWidth(text,font))/2,y,text,font,color);}
inline void icon(ui::Canvas &c,int cy){
  c.rect(92,cy-51,96,3,Accent);c.rect(92,cy-51,3,99,Accent);c.rect(185,cy-51,3,99,Accent);c.rect(89,cy+46,102,5,Accent);
  c.rect(95,cy-27,90,3,Accent);c.rect(131,cy-30,18,15,Accent);c.line(140,cy-15,140,cy-6,Accent);
  c.rect(114,cy+14,52,3,Ink);c.rect(114,cy+22,52,3,Ink);c.rect(114,cy+30,52,3,Ink);
}
inline const char *linkLabel(const Snapshot &s,uint32_t now){
  if(fresh(s,now))return "LIVE";
  switch(s.link){case Link::Auth:return "ACCESS CODE";case Link::Certificate:return "CERTIFICATE";case Link::Wifi:return "WI-FI OFFLINE";case Link::Clock:return "SYNCING TIME";case Link::Offline:return "DISCONNECTED";case Link::Connecting:return "CONNECTING";case Link::Error:return "RETRYING";default:return s.revision?"STALE":"WAITING";}
}
inline void render(ui::Canvas &c,const Snapshot &s,unsigned page,uint32_t now,int64_t epoch){
  c.clear();c.text(20,12,"BAMBU LAB",ui::fontBody,Accent);c.right(260,16,"P1S",ui::fontSmall,Ink);
  bool setup=s.link==Link::Setup||s.link==Link::InvalidConfig||s.link==Link::Disabled;
  if(setup||!s.revision){
    c.rect(20,47,240,1,Grid);icon(c,160);
    const char *title="CONNECTING",*line1="WAITING FOR YOUR PRINTER",*line2="RECONNECTS AUTOMATICALLY";
    switch(s.link){
      case Link::Setup:title="ADD YOUR PRINTER";line1="SAVE PRINTER.TXT TO SD";line2="THEN RESTART THE DISPLAY";break;
      case Link::InvalidConfig:title="CHECK PRINTER.TXT";line1="IP / SERIAL / ACCESS CODE";line2="SAVE AND RESTART DISPLAY";break;
      case Link::Disabled:title="PRINTER DISABLED";line1="SET ENABLED=TRUE ON SD";line2="THEN RESTART THE DISPLAY";break;
      case Link::Wifi:title="WI-FI OFFLINE";line1="WAITING FOR CONNECTION";break;
      case Link::Clock:title="SYNCING TIME";line1="WAITING FOR NETWORK TIME";break;
      case Link::Auth:title="CHECK ACCESS CODE";line1="UPDATE PRINTER.TXT ON SD";line2="THEN RESTART THE DISPLAY";break;
      case Link::Certificate:title="CHECK PRINTER ID";line1="CERTIFICATE CHECK FAILED";line2="VERIFY SERIAL AND CLOCK";break;
      case Link::Waiting:title="WAITING FOR STATUS";line1="CONNECTED TO YOUR P1S";line2="REQUESTING PRINTER UPDATE";break;
      case Link::Offline:case Link::Error:title="PRINTER OFFLINE";line1="CHECK PRINTER POWER / WI-FI";break;
      default:break;
    }
    fitted(c,20,246,title,ui::fontHeading,Ink);c.text(20,292,line1,ui::fontSmall,Ink);c.text(20,324,line2,ui::fontSmall,Ink);
    c.text(20,385,"LOCAL STATUS",ui::fontBody,Accent);c.text(20,425,"SWIPE LEFT / RIGHT FOR WIDGETS",ui::fontSmall,Ink);return;
  }
  bool live=fresh(s,now);fitted(c,20,49,live?(s.error?"PRINTER ERROR":stateLabel(s.state)):"LAST KNOWN STATUS",ui::fontHeading,live?Accent:Ink);
  char value[96];
  if(page%PageCount==0){
    constexpr float cx=140,cy=186,r=88;
    constexpr int ringWidth=6;
    for(int j=0;j<ringWidth;j++)c.circle(cx,cy,r-j,Grid);
    if(s.state==State::Idle){icon(c,186);}
    else {
      int percent=s.progress>=0?std::min(s.progress,100):0;
      for(int deg=0;deg<percent*360/100;deg++){float a=(deg-90)*ui::Pi/180,b=(deg-89)*ui::Pi/180;for(int j=0;j<ringWidth;j++)c.line(cx+cosf(a)*(r-j),cy+sinf(a)*(r-j),cx+cosf(b)*(r-j),cy+sinf(b)*(r-j),Accent);}
      if(s.progress<0)strcpy(value,"--");else snprintf(value,sizeof(value),"%d",s.progress);
      int w=c.textWidth(value,ui::priceFont),x=(ui::Width-w)/2;
      int top=76,bottom=0;for(const char *p=value;*p;p++){const auto &g=ui::priceFont.glyphs[*p-32];top=std::min(top,int(g.dy));bottom=std::max(bottom,int(g.dy+g.h));}
      // Center the large digits on their own visible bounds; the small unit is a suffix.
      c.text(x,int(cy)-(top+bottom)/2,value,ui::priceFont,Ink);c.text(x+w+5,181,"%",ui::fontHeading,Accent);
    }
    center(c,296,s.state==State::Idle?"READY FOR A PRINT":"TIME REMAINING",ui::fontSmall,Ink);
    durationLabel(s.state==State::Idle?-1:s.remaining,value,sizeof(value));center(c,323,value,ui::fontNumber,Ink);
    if(live&&printing(s)&&s.remaining>=0&&s.remainingEpoch>1700000000){
      time_t finish=time_t(s.remainingEpoch+int64_t(s.remaining)*60);struct tm local;
#ifdef _WIN32
      localtime_s(&local,&finish);
#else
      localtime_r(&finish,&local);
#endif
      char clock[24];strftime(clock,sizeof(clock),"%I:%M %p",&local);snprintf(value,sizeof(value),"FINISH %s",clock[0]=='0'?clock+1:clock);center(c,366,value,ui::fontBody,Accent);
    }else if(s.state==State::Paused)center(c,366,"ESTIMATE PAUSED",ui::fontBody,Ink);
    else if(s.state==State::Finished)center(c,366,"PRINT COMPLETE",ui::fontBody,Accent);
    else if(!live)center(c,366,"WAITING FOR FRESH DATA",ui::fontSmall,Ink);
  }else{
    c.text(20,102,"NOZZLE / F",ui::fontSmall,Ink);c.text(158,102,"BED / F",ui::fontSmall,Ink);
    if(isfinite(s.nozzle))snprintf(value,sizeof(value),"%.0f",double(fahrenheit(s.nozzle)));else strcpy(value,"--");c.text(19,132,value,ui::fontNumber,Ink);
    if(isfinite(s.bed))snprintf(value,sizeof(value),"%.0f",double(fahrenheit(s.bed)));else strcpy(value,"--");c.text(157,132,value,ui::fontNumber,Ink);
    auto target=[&](int x,float temp){if(!isfinite(temp))strcpy(value,"TARGET --");else if(temp==0)strcpy(value,"HEATER OFF");else snprintf(value,sizeof(value),"TARGET %.0f F",double(fahrenheit(temp)));c.text(x,181,value,ui::fontSmall,Ink);};target(20,s.nozzleTarget);target(158,s.bedTarget);
    c.rect(20,216,240,1,Grid);c.text(20,237,"LAYER",ui::fontSmall,Ink);
    if(s.layer>=0&&s.totalLayers>=0)snprintf(value,sizeof(value),"%d / %d",s.layer,s.totalLayers);else if(s.layer>=0)snprintf(value,sizeof(value),"%d / --",s.layer);else strcpy(value,"-- / --");fitted(c,20,269,value,ui::fontNumber,Accent);
    c.text(20,328,"PRINT",ui::fontSmall,Ink);fitted(c,20,357,s.name[0]?s.name:"NO PRINT NAME",ui::fontBody,Ink);
  }
  c.rect(20,399,240,1,Grid);snprintf(value,sizeof(value),"%u / %u",page%PageCount+1,PageCount);c.text(20,410,value,ui::fontBody,Ink);c.right(260,410,linkLabel(s,now),ui::fontBody,live?Accent:Ink);
}
}
