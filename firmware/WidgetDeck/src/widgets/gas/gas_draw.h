#pragma once
#include "../../core/canvas.h"
namespace gas {using ui::Canvas;using ui::Font;using ui::Glyph;using ui::rgb;using ui::fontSmall;using ui::fontBody;using ui::fontHeading;using ui::fontNumber;using ui::fontTitle;}
#include "gas_data.h"
#include "gas_font.h"

namespace gas {
static const uint16_t Green=rgb(174,234,147),Ink=rgb(255,255,255),Grid=rgb(29,43,34);
inline void render(Canvas &c,const View &view) {
  c.clear();
  const int regionLimit=260-c.textWidth("REGULAR",fontSmall)-12;
  const int regionSpacing=c.textWidth(view.regionName,fontSmall,2)<=regionLimit-20?2:0;
  c.text(20,16,view.regionName,fontSmall,Green,regionSpacing,regionLimit);
  c.right(260,16,"REGULAR",fontSmall,Ink);
  c.rect(20,46,240,1,Grid);
  if(!view.history.count) {
    c.text(35,104,"--.---",priceFont,Ink,0,265);
    const char *title="GETTING PRICES",*line1="LOADING WEEKLY HISTORY",*line2="FROM U.S. EIA";
    if(view.state==State::Setup){title="CONNECT WI-FI";line1="ADD WIFI.TXT TO YOUR SD";line2="INSERT CARD AND RESTART";}
    else if(view.state==State::MissingKey){title="ADD API KEY";line1="SET GAS_EIA_API_KEY";line2="IN SECRETS.H AND REBUILD";}
    else if(view.state==State::Offline){title="WI-FI OFFLINE";line1="RETRYING WI-FI CONNECTION";line2="CHECK WIFI.TXT ON YOUR SD";}
    else if(view.state==State::Clock){title="SYNCING TIME";line1="WAITING FOR NETWORK TIME";line2="FOR A SECURE CONNECTION";}
    else if(view.state==State::Error){title="EIA UNAVAILABLE";line1="RETRYING AUTOMATICALLY";line2="TAP TO TRY AGAIN";}
    c.text(20,241,title,fontHeading,Ink);c.text(20,288,line1,fontSmall,Ink);c.text(20,314,line2,fontSmall,Ink);
    c.rect(20,411,240,1,Grid);c.text(20,424,"EIA / WEEKLY AVERAGE",fontSmall,Ink);return;
  }
  const History &h=view.history;char label[64];const Point &last=h.latest();
  snprintf(label,sizeof(label),"%.3f",double(last.price));
  const Font &font=c.textWidth(label,priceFont)>214?fontTitle:priceFont;
  int left=(280-c.textWidth(label,font)-20)/2;
  c.text(left,98,"$",fontNumber,Green);c.text(left+22,81,label,font,Ink,0,266);
  dateLabel(last.day,label,sizeof(label));c.text(20,172,label,fontSmall,Ink);
  if(h.count>1&&last.day-h.points[h.count-2].day==7){snprintf(label,sizeof(label),"%+.1fc / WK",double((last.price-h.points[h.count-2].price)*100));c.right(260,172,label,fontSmall,Green);}
  c.rect(20,207,240,1,Grid);c.text(20,218,"3 YEAR HISTORY",fontSmall,Ink,1);
  snprintf(label,sizeof(label),"%u %s",unsigned(h.count),h.count==1?"WK":"WKS");c.right(260,218,label,fontSmall,Ink);
  const Scale scale=chartScale(h);const int x0=47,x1=255,y0=264,y1=367;
  const int start=threeYearsBefore(last.day),end=last.day;
  auto x=[&](int day){return x0+(x1-x0)*float(day-start)/(end-start);};
  auto y=[&](float price){return y1-(y1-y0)*(price-scale.low)/(scale.high-scale.low);};
  for(int i=0;i<3;i++){
    float v=scale.low+(scale.high-scale.low)*i/2;float yy=y(v);
    c.line(x0,yy,x1,yy,Grid);snprintf(label,sizeof(label),v>=10?"%.1f":"%.2f",double(v));c.right(39,int(yy)-9,label,fontSmall,Ink);
  }
  for(unsigned i=1;i<h.count;i++)if(h.points[i].day-h.points[i-1].day==7) {
    float xa=x(h.points[i-1].day),xb=x(h.points[i].day),ya=y(h.points[i-1].price),yb=y(h.points[i].price);
    for(int xx=int(ceilf(xa));xx<=int(xb);xx++){
      float yy=ya+(yb-ya)*(xx-xa)/(xb-xa);
      for(int row=int(ceilf(yy));row<y1;row++)c.pixel(xx,row,Green,int(26*(y1-row)/float(std::max(1,y1-int(yy))))+3);
    }
    c.line(xa,ya,xb,yb,Green,210);
  }
  // Every observation gets its own dot; gaps are not connected.
  for(unsigned i=0;i<h.count;i++)c.circle(x(h.points[i].day),y(h.points[i].price),.8f,Green);
  c.circle(x1,y(last.price),3.1f,Green);c.circle(x1,y(last.price),1.1f,Ink);
  Date first=dateFromDay(start);
  for(int i=0;i<4;i++) {
    Date d=first;d.year+=i;d.day=std::min(d.day,monthDays(d.year,d.month));
    int xx=int(x(dayNumber(d)));c.line(xx,y1,xx,y1+4,Grid);
    snprintf(label,sizeof(label),"%d",d.year);c.text(xx-17,377,label,fontSmall,Ink,0,277);
  }
  c.rect(20,411,240,1,Grid);c.text(20,424,"EIA / TAX INCLUDED",fontSmall,Ink);
  bool old=view.today>last.day+14;
  const char *status=old?"OLDER":view.state==State::Current?"WEEKLY":"CACHED";
  c.right(260,424,status,fontSmall,old?rgb(255,188,105):Green);
}
}
