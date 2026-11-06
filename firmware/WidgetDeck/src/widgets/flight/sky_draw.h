#pragma once
#include "sky_data.h"
#include "sky_identity.h"
#include "../../core/canvas.h"
namespace sky {
using ui::rgb;using ui::Point;using ui::Font;using ui::fontSmall;using ui::fontBody;using ui::fontHeading;using ui::fontNumber;using ui::fontTitle;
static const uint16_t White=rgb(255,255,255),Muted=rgb(115,143,151),Dim=rgb(41,62,69),Accent=rgb(140,213,255);
// Right-half wing/stabilizer outline: root at x=rx, leading/trailing edge y at the
// root, tip and a trailing-edge kink. Used for planform drawing and service bands.
struct PlanWing {
  float rx,le0,tx,tle,tte,kx,kte,te0;
  float le(float x)const{return le0+(tle-le0)*(x-rx)/(tx-rx);}
  float te(float x)const{return x<kx?te0+(kte-te0)*(x-rx)/(kx-rx):kte+(tte-kte)*(x-kx)/(tx-kx);}
};
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
    // Plan-view silhouettes of each aircraft class (proportions follow typical
    // types, but they are class drawings, not exact models). Every shape is built
    // from a mirrored half outline so it stays symmetric. The radar shows bearing.
    const float angle=32*float(Pi)/180,ca=cosf(angle),sa=sinf(angle);
    auto rot=[&](float x,float y)->Point{return {cx+x*ca-y*sa,165+x*sa+y*ca};};
    auto shape=[&](const Point *pts,int n,uint16_t c){Point p[48];for(int i=0;i<n;i++)p[i]=rot(pts[i].x,pts[i].y);polygon(p,n,c);};
    auto mirror=[&](const Point *half,int n,uint16_t c){Point p[48];for(int i=0;i<n;i++){p[i]=half[i];p[2*n-1-i]={-half[i].x,half[i].y};}shape(p,2*n,c);};
    auto stroke=[&](float x0,float y0,float x1,float y1,uint16_t c,int alpha=255){Point a=rot(x0,y0),b=rot(x1,y1);line(a.x,a.y,b.x,b.y,c,alpha);};
    bool special=service!=Role::Standard;
    uint16_t color=serviceColor(service),bodyColor=service==Role::Military?rgb(164,183,137):White;
    uint16_t wingColor=service==Role::Military?rgb(119,140,108):rgb(176,199,207);
    uint16_t tailColor=special?color:rgb(136,163,176),engineColor=rgb(118,143,155),glassColor=rgb(84,112,125);
    auto marking=[&](float x,float y){
      if(service==Role::Military){Point star[10];for(int i=0;i<10;i++){float t=i*float(Pi)/5-float(Pi)/2,r=i%2?2.4f:5.8f;star[i]={x+cosf(t)*r,y+sinf(t)*r};}shape(star,10,White);}
      else if(service==Role::Rescue){const Point cross[]={{x-1.6f,y-5},{x+1.6f,y-5},{x+1.6f,y-1.6f},{x+5,y-1.6f},{x+5,y+1.6f},{x+1.6f,y+1.6f},{x+1.6f,y+5},{x-1.6f,y+5},{x-1.6f,y+1.6f},{x-5,y+1.6f},{x-5,y-1.6f},{x-1.6f,y-1.6f}};shape(cross,12,White);}
    };
    if(kind==2) {
      // Helicopter: cabin, tail boom, skids, tail rotor and a spinning main rotor.
      const Point cabin[]={{0,-38},{6,-35},{9.5f,-26},{10,-8},{8,6},{4,18},{2.6f,24}};
      const Point boom[]={{2.6f,18},{2.2f,58},{-2.2f,58},{-2.6f,18}};
      const Point fin[]={{2.2f,48},{9,56},{9,60},{2.2f,58}};
      for(int side:{-1,1}){stroke(side*13.f,-26,side*13.f,12,special?wingColor:Muted);stroke(side*13.f,-6,side*9.f,-6,Muted);}
      mirror(cabin,7,bodyColor);shape(boom,4,bodyColor);shape(fin,4,tailColor);
      stroke(7,50,7,66,Muted);
      Point hub=rot(0,-6);circle(hub.x,hub.y,56,Muted,60);
      {const float ux=.88f,uy=.47f,nx=-uy*1.7f,ny=ux*1.7f;
       const Point blade[]={{-ux*54+nx,-6-uy*54+ny},{ux*54+nx,-6+uy*54+ny},{ux*54-nx,-6+uy*54-ny},{-ux*54-nx,-6-uy*54-ny}};shape(blade,4,Muted);}
      if(special){
        const Point panel[]={{-8,-22},{8,-22},{9,-8},{-9,-8}};shape(panel,4,color);
        const Point glass[]={{-5,-33},{0,-37},{5,-33},{6,-27},{-6,-27}};shape(glass,5,glassColor);
        if(service==Role::Fire){const Point tank[]={{-9,0},{9,0},{7,14},{-7,14}};shape(tank,4,color);}
        marking(0,-15);
      }
      circle(hub.x,hub.y,5,special?color:Accent);
      return;
    }
    // Fixed-wing classes. Wing/stabilizer outlines give the right half; y grows
    // toward the tail. Table order matches silhouette() in sky_data.h.
    struct Nacelle{float x,y0,y1,r;bool prop;};
    struct Spec{float hw,nose,tail,noseLen,tailLen,tip;PlanWing wing,stab;bool tTail;float noseProp;int nacelles;Nacelle nacelle[2];};
    static const Spec specs[]={
      /*0 narrow-body jet*/{6.5f,-76,76,22,40,1.4f,{6.5f,-20,70,13,25,28,18,8},{3,52,27,63,70,12,68,64},false,0,1,{{24,-28,-3,4.8f,false}}},
      /*1 light single prop*/{6.f,-44,46,12,30,1.f,{4.8f,-14,58,-12,3,30,3.5f,4},{2,34,19,38,44,8,43,42},false,13,0,{}},
      /*2 helicopter (drawn above)*/{},
      /*3 wide-body twin jet*/{9,-86,90,26,48,1.6f,{9,-20,78,24,40,32,31,16},{4,60,30,76,84,14,80,78},false,0,1,{{28,-33,0,6.4f,false}}},
      /*4 four-engine jet*/{9.5f,-90,92,26,50,1.6f,{9.5f,-22,80,32,50,36,38,22},{4,62,32,78,88,14,82,80},false,0,2,{{26,-34,-3,5.4f,false},{50,-16,13,5.2f,false}}},
      /*5 regional jet*/{5.2f,-62,62,18,34,1.2f,{5.2f,-12,47,5,17,18,13,8},{2.2f,47,19,53,60,8,57,56},true,0,1,{{10.5f,24,46,4.f,false}}},
      /*6 business jet*/{4.4f,-50,50,15,28,1.f,{4.4f,-8,42,10,21,16,15,8},{2,36,17,41,47,6,45,44},true,0,1,{{8.2f,18,36,3.4f,false}}},
      /*7 twin turboprop*/{5.5f,-60,60,16,34,1.3f,{5.5f,-12,60,-8,3,24,7,8},{2.5f,46,24,50,57,10,56,54},true,0,1,{{22,-30,8,4.2f,true}}},
      /*8 four-engine turboprop*/{8,-66,70,18,36,3,{8,-14,72,-10,8,28,10,6},{3,52,26,58,68,12,64,62},true,0,2,{{24,-32,4,4.4f,true},{44,-30,6,4.4f,true}}}
    };
    const Spec &s=specs[kind>=0&&kind<9&&kind!=2?kind:0];
    auto wingShape=[&](const PlanWing &w,uint16_t c){const Point h[5]={{w.rx,w.le0},{w.tx,w.tle},{w.tx,w.tte},{w.kx,w.kte},{w.rx,w.te0}};mirror(h,5,c);};
    auto nacelles=[&](){
      for(int i=0;i<s.nacelles;i++)for(int side:{-1,1}){
        const Nacelle &n=s.nacelle[i];float x=side*n.x;
        const Point pod[]={{x-n.r,n.y0+n.r*.8f},{x,n.y0},{x+n.r,n.y0+n.r*.8f},{x+n.r,n.y1},{x-n.r,n.y1}};
        shape(pod,5,engineColor);
        if(n.prop)stroke(x-n.r*2.f,n.y0-1,x+n.r*2.f,n.y0-1,Muted);
      }
    };
    const bool under=kind==0||kind==3||kind==4;  // under-wing jet engines only show ahead of the wing
    if(!s.tTail)wingShape(s.stab,tailColor);
    if(under)nacelles();
    wingShape(s.wing,wingColor);
    if(special){ // service-colored wing bands and markings
      float x0=s.wing.rx+(s.wing.tx-s.wing.rx)*.45f,x1=s.wing.rx+(s.wing.tx-s.wing.rx)*.8f;
      for(int side:{-1,1}){const Point band[]={{side*x0,s.wing.le(x0)},{side*x1,s.wing.le(x1)},{side*x1,s.wing.te(x1)},{side*x0,s.wing.te(x0)}};shape(band,4,color);}
      float xm=(x0+x1)/2,ym=(s.wing.le(xm)+s.wing.te(xm))/2;marking(-xm,ym);marking(xm,ym);
    }
    if(!under)nacelles();
    const Point body[]={{0,s.nose},{s.hw*.5f,s.nose+s.noseLen*.18f},{s.hw*.88f,s.nose+s.noseLen*.55f},{s.hw,s.nose+s.noseLen},{s.hw,s.tail-s.tailLen},{s.tip,s.tail},{0,s.tail+1}};
    mirror(body,7,bodyColor);
    if(s.tTail)wingShape(s.stab,tailColor);
    if(s.noseProp>0)stroke(-s.noseProp,s.nose-2,s.noseProp,s.nose-2,Muted);
    // Cockpit windscreen marks the nose.
    const Point glass[]={{-s.hw*.55f,s.nose+s.noseLen*.62f},{0,s.nose+s.noseLen*.5f},{s.hw*.55f,s.nose+s.noseLen*.62f},{s.hw*.4f,s.nose+s.noseLen*.8f},{-s.hw*.4f,s.nose+s.noseLen*.8f}};
    shape(glass,5,glassColor);
    if(special){const float y=s.wing.te0+6;const Point stripe[]={{-s.hw,y},{s.hw,y},{s.hw,y+9},{-s.hw,y+9}};shape(stripe,4,color);}
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
