#pragma once
#include "sky_data.h"
#include "sky_identity.h"
#include "../../core/canvas.h"
namespace sky {
using ui::rgb;using ui::Point;using ui::Font;using ui::fontSmall;using ui::fontBody;using ui::fontHeading;using ui::fontNumber;using ui::fontTitle;
static const uint16_t White=rgb(255,255,255),Muted=rgb(115,143,151),Dim=rgb(41,62,69),Accent=rgb(140,213,255);
class Canvas:public ui::Canvas {
 public:
  using ui::Canvas::Canvas;
  uint16_t serviceColor(Role role)const {
    switch(role){case Role::Military:return rgb(195,211,151);case Role::Police:return rgb(91,167,255);
      case Role::Rescue:return rgb(255,179,72);case Role::Fire:return rgb(255,102,65);default:return Accent;}
  }
  void identity(const Aircraft &a,Role service) {
    const Airline *brand=airline(a);
    if(brand)for(int y=0;y<40;y++)for(int x=0;x<brand->width;x++){
      int i=y*brand->width+x;uint8_t bits=brand->mask[i/2];int alpha=((i&1)?bits&15:bits>>4)*17;
      pixel(36-brand->width/2+x,145+y,White,alpha);
    }
    if(service==Role::Standard)return;
    uint16_t color=serviceColor(service);
    text(20,90,roleName(service),fontSmall,White);
    auto badge=[&](const Point *pts,int n,uint16_t c){Point p[24];for(int i=0;i<n;i++)p[i]={36+pts[i].x,165+pts[i].y};polygon(p,n,c);};
    if(service==Role::Military){
      Point star[10];for(int i=0;i<10;i++){float t=i*float(Pi)/5-float(Pi)/2,r=i%2?7:17;star[i]={cosf(t)*r,sinf(t)*r};}badge(star,10,color);
    }else if(service==Role::Police){
      const Point shield[]={{-17,-16},{0,-20},{17,-16},{15,5},{8,14},{0,20},{-8,14},{-15,5}};badge(shield,8,color);
      Point star[10];for(int i=0;i<10;i++){float t=i*float(Pi)/5-float(Pi)/2,r=i%2?4:9;star[i]={cosf(t)*r,sinf(t)*r-2};}badge(star,10,White);
    }else if(service==Role::Rescue){
      rect(19,148,34,34,color);rect(31,153,10,24,White);rect(24,160,24,10,White);
    }else{
      const Point flame[]={{-16,8},{-15,-3},{-8,-12},{-6,-3},{3,-21},{13,-10},{18,3},{14,14},{3,20},{-8,18}};badge(flame,10,color);
      const Point core[]={{-6,10},{-3,1},{0,5},{5,-3},{8,9},{3,15},{-2,16}};badge(core,7,rgb(255,210,99));
    }
  }
  void plane(int kind,Role service=Role::Standard,float cx=140) {
    // Category silhouettes, deliberately not presented as exact model drawings.
    // Keep the large illustration composed consistently; the radar shows actual bearing.
    float angle=32*float(Pi)/180;
    auto shape=[&](const Point *pts,int n,uint16_t c){Point p[48];for(int i=0;i<n;i++)p[i]={cx+pts[i].x*cosf(angle)-pts[i].y*sinf(angle),165+pts[i].x*sinf(angle)+pts[i].y*cosf(angle)};polygon(p,n,c);};
    bool special=service!=Role::Standard;
    uint16_t color=serviceColor(service),bodyColor=service==Role::Military?rgb(164,183,137):White;
    uint16_t wingColor=service==Role::Military?rgb(119,140,108):rgb(178,201,208);
    auto marking=[&](float x,float y){
      if(service==Role::Military){Point star[10];for(int i=0;i<10;i++){float t=i*float(Pi)/5-float(Pi)/2,r=i%2?2.4f:5.8f;star[i]={x+cosf(t)*r,y+sinf(t)*r};}shape(star,10,White);}
      else if(service==Role::Rescue){const Point cross[]={{x-1.6f,y-5},{x+1.6f,y-5},{x+1.6f,y-1.6f},{x+5,y-1.6f},{x+5,y+1.6f},{x+1.6f,y+1.6f},{x+1.6f,y+5},{x-1.6f,y+5},{x-1.6f,y+1.6f},{x-5,y+1.6f},{x-5,y-1.6f},{x-1.6f,y-1.6f}};shape(cross,12,White);}
    };
    if(kind==2) {
      const Point body[]={{0,-39},{10,-29},{12,-8},{7,13},{3,56},{-3,56},{-7,13},{-12,-8},{-10,-29}};
      const Point rotor[]={{-76,-2.5f},{-76,2.5f},{76,2.5f},{76,-2.5f}};
      const Point tail[]={{-20,48},{20,48},{20,51},{-20,51}};
      if(special)for(int side:{-1,1}){Point skid[]={{float(side*18-1),-24},{float(side*18+1),-24},{float(side*18+1),22},{float(side*18-1),22}};shape(skid,4,wingColor);}
      shape(tail,4,special?color:Muted);shape(body,9,bodyColor);
      if(special){
        const Point panel[]={{-9,-25},{9,-25},{11,-8},{-11,-8}};shape(panel,4,color);
        const Point glass[]={{-5,-31},{0,-36},{5,-31},{7,-26},{-7,-26}};shape(glass,5,Dim);
        if(service==Role::Fire){const Point tank[]={{-12,7},{12,7},{10,22},{-10,22}};shape(tank,4,color);}
        marking(0,-17);
      }
      shape(rotor,4,Muted);circle(cx,165,5,special?color:Accent);
    } else if(kind==1) {
      const Point body[]={{0,-61},{5,-49},{7,24},{3,61},{-3,61},{-7,24},{-5,-49}};
      const Point wings[]={{-70,-15},{70,-15},{70,0},{-70,0}};
      const Point tail[]={{-27,42},{27,42},{27,50},{-27,50}};
      shape(wings,4,wingColor);shape(tail,4,special?color:Muted);shape(body,7,bodyColor);
      if(special){
        for(int side:{-1,1}){float x=side*47;Point band[]={{x-9,-15},{x+9,-15},{x+9,0},{x-9,0}};shape(band,4,color);marking(x,-7.5f);}
        const Point stripe[]={{-6,14},{6,14},{5,28},{-5,28}};shape(stripe,4,color);
      }
      const Point prop[]={{-22,-54},{22,-54},{22,-52},{-22,-52}};shape(prop,4,Muted);
    } else {
      const Point wings[]={{-7,-24},{-79,15},{-83,30},{-6,9},{6,9},{83,30},{79,15},{7,-24}};
      const Point tail[]={{-4,43},{-31,63},{-31,70},{0,62},{31,70},{31,63},{4,43}};
      const Point body[]={{0,-76},{4,-69},{7,-54},{7,26},{4,63},{0,71},{-4,63},{-7,26},{-7,-54},{-4,-69}};
      shape(wings,8,special?wingColor:rgb(168,194,204));shape(tail,7,special?color:rgb(136,163,176));
      if(special)for(int side:{-1,1}){Point band[]={{float(side*37),-7},{float(side*58),4},{float(side*61),24},{float(side*40),18}};shape(band,4,color);marking(side*47,8);}
      for(int side:{-1,1}){Point engine[]={{float(side*28-4),-13},{float(side*28+4),-13},{float(side*28+4),10},{float(side*28-4),10}};shape(engine,4,bodyColor);}
      shape(body,10,bodyColor);const Point seam[]={{0,-62},{2,-53},{2,40},{0,63}};shape(seam,4,special?color:rgb(196,216,224));
      if(special){const Point stripe[]={{-7,13},{7,13},{7,26},{-7,26}};shape(stripe,4,color);}
    }
  }
  void radar(float bearing) {
    int cx=239,cy=358;circle(cx,cy,18,Dim);circle(cx,cy,9,Dim,140);
    line(cx-24,cy,cx+24,cy,Dim,150);line(cx,cy-24,cx,cy+24,Dim,150);
    float r=bearing*float(Pi)/180,dx=sinf(r),dy=-cosf(r);
    Point arrow[]={{cx+dx*18,cy+dy*18},{cx-dx*4-dy*4,cy-dy*4+dx*4},{cx-dx*4+dy*4,cy-dy*4-dx*4}};
    polygon(arrow,3,Accent);
  }
  void card(const Slideshow &show,const Snapshot &snapshot,const Home &home,uint32_t now,bool demo=false,int slideOffset=0) {
    clear();offset=slideOffset;const Aircraft *a=show.current();if(!a){offset=0;return;}
    char value[64];
    const char *state=demo?"DEMO":snapshot.status==Status::Live?(snapshot.localSource?"LOCAL":"LIVE"):"CACHED";
    const char *id=callsign(*a);const Font &title=textWidth(id,fontTitle)>240?fontNumber:fontTitle;
    text(19,13,id,title,White);text(20,64,model(*a),fontBody,White);
    Role service=role(*a);const Airline *brand=airline(*a);
    int cx=brand?(brand->width>40?160:150):service!=Role::Standard?150:140;
    circle(cx,165,72,Dim,95);circle(cx,165,48,Dim,70);
    for(int i=0;i<12;i++){float t=i*float(Pi)/6;line(cx+sinf(t)*76,165+cosf(t)*76,cx+sinf(t)*79,165+cosf(t)*79,Dim,140);}
    identity(*a,service);plane(silhouette(*a),service,cx);
    text(20,251,"ALT / FT",fontSmall,White);text(158,251,"DIST / MI",fontSmall,White);
    if(isfinite(a->altitude)) {int alt=int(roundf(a->altitude));if(abs(alt)>=1000)snprintf(value,sizeof(value),"%d,%03d",alt/1000,abs(alt%1000));else snprintf(value,sizeof(value),"%d",alt);}else strcpy(value,"--");
    // Align the visible tops across font sizes, leaving space below each label.
    text(19,277,value,fontNumber,White);snprintf(value,sizeof(value),"%.1f",a->distance);text(157,276,value,fontTitle,Accent);
    // Simple altitude trend arrow, legible without another line of small text.
    if(isfinite(a->verticalRate)&&fabsf(a->verticalRate)>150){int x=139,y=297,sign=a->verticalRate>0?-1:1;line(x,y-7*sign,x,y+7*sign,Muted);line(x,y+7*sign,x-4,y+3*sign,Muted);line(x,y+7*sign,x+4,y+3*sign,Muted);}
    text(20,324,"SPEED",fontSmall,White);text(158,324,"LOOK",fontSmall,White);
    if(isfinite(a->speed))snprintf(value,sizeof(value),"%d KT",int(roundf(a->speed)));else strcpy(value,"-- KT");
    text(20,348,value,fontHeading,White);
    text(157,347,compass(a->bearing),fontNumber,Accent,0,213);radar(a->bearing);
    rect(20,385,240,2,Dim);rect(20,385,int(240*show.progress(now)),2,Accent);
    snprintf(value,sizeof(value),"%d / %d",show.index+1,show.count);text(20,397,value,fontBody,White);
    right(260,397,state,fontBody,demo?White:Accent);
    const char *hint="TAP TO TRACK";
    text((Width-textWidth(hint,fontSmall))/2,427,hint,fontSmall,White);
    offset=0;
  }
  void message(const char *title,const char *line1,const char *line2,const Home &home,Status status,bool localSource=false) {
    clear();text(20,20,"NEARBY SKY",fontSmall,White,2);right(260,20,status==Status::Live?(localSource?"LOCAL":"LIVE"):"STATUS",fontSmall,Accent,1);rect(20,44,240,1,Dim);
    circle(140,164,60,Dim);circle(140,164,30,Dim);line(75,164,205,164,Dim);line(140,99,140,229,Dim);
    circle(140,164,4,Accent);text(20,254,title,textWidth(title,fontNumber)>240?fontHeading:fontNumber,White);
    text(20,301,line1,fontBody,White);text(20,325,line2,fontBody,White);
    char value[64];snprintf(value,sizeof(value),"%.0F MI SEARCH RADIUS",double(home.radius));text(20,382,value,fontSmall,White,1);
    text(20,409,"HOLD FOR SETTINGS",fontSmall,Accent);text(20,432,localSource?"LOCAL / MESHPOINT":"ADSB.LOL / ODBL",fontSmall,White);
  }
  void setup(const char *ssid,const char *password,const Home &home) {
    clear();text(20,20,"WIDGET DECK",fontSmall,White,2);rect(20,44,240,1,Dim);
    text(20,60,"LET'S CONNECT",fontNumber,White);
    text(20,116,"1 / JOIN THIS WI-FI",fontSmall,Accent,1);text(20,143,ssid,fontBody,White);
    text(20,185,"PASSWORD",fontSmall,White,1);text(20,207,password,fontNumber,White);
    text(20,267,"2 / OPEN IN YOUR BROWSER",fontSmall,Accent,1);text(20,294,"192.168.4.1",fontNumber,White);
    text(20,344,"ADD YOUR HOME WI-FI",fontBody,White);text(20,370,"LOCATION IS PRE-FILLED",fontSmall,White,1);
    text(20,421,"HOLD TO RETURN TO WIDGETS",fontSmall,White);
  }
};
}
