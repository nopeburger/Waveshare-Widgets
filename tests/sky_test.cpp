#ifdef NDEBUG
#undef NDEBUG
#endif
#ifdef WIDGET_DECK
#include "../firmware/WidgetDeck/src/widgets/flight/sky_feed.h"
#include "../firmware/WidgetDeck/src/widgets/flight/sky_draw.h"
#include "../firmware/WidgetDeck/src/core/sky_settings.h"
#else
#include "../firmware/AmbientDay/sky_feed.h"
#include "../firmware/AmbientDay/sky_draw.h"
#include "../firmware/AmbientDay/sky_settings.h"
#endif
#include <assert.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>

static std::string read(const char *path){std::ifstream f(path);std::ostringstream b;b<<f.rdbuf();return b.str();}
static bool parse(const std::string &s,sky::Snapshot &out,double epoch=1789670000,uint32_t now=1000,sky::Home home={}){return sky::parseFeed(s.c_str(),s.size(),home,epoch,now,out);}
static void ppm(const char *path,const uint16_t *pixels) {
  FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n280 456\n255\n");
  for(int i=0;i<280*456;i++){uint16_t p=pixels[i];unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);
}
int main(int argc,char **argv) {
  using namespace sky;Home home;
  CardSettings settings;
  std::string ini="\xEF\xBB\xBF# Config\r\nssid=My home\r\npassword=abc#123=def \r\nlatitude=37.7749\r\nlongitude=-122.4194\r\nradius_miles=10\r\n";
  assert(parseSettings(ini.data(),ini.size(),settings));assert(!strcmp(settings.password,"abc#123=def "));assert(!strcmp(settings.ssid,"My home"));
  for(const char *bad:{"ssid=Home\n","ssid=Home\npassword=short\n","ssid=Home\npassword=abcdefgh\nlatitude=nan\n","ssid=Home\npassword=abcdefgh\nradius_miles=0\n","ssid=Home\nssid=Other\npassword=abcdefgh\n"})assert(!parseSettings(bad,strlen(bad),settings));
  const char *open="ssid=Open network\npassword=\n";assert(parseSettings(open,strlen(open),settings));assert(!settings.password[0]);
  assert(validHome(home));assert(!validHome(Home{91,0,10}));assert(!validHome(Home{0,0,NAN}));
  assert(distanceMiles(home,home.lat,home.lon)<.0001);
  assert(fabsf(distanceMiles(Home{0,0,10},1,0)-69.0934f)<.01f);
  assert(fabsf(bearingDegrees(Home{0,0,10},0,1)-90)<.01);
  assert(distanceMiles(Home{0,179.99,10},0,-179.99)<1.4);
  const std::string good=R"({"now":1789670000000,"ac":[
    {"hex":"abcd01","flight":"TEST1   ","t":"B738","lat":37.775,"lon":-122.419,"seen_pos":2,"alt_baro":12300,"gs":245.5,"track":42},
    {"hex":"abcd02","lat":37.780,"lon":-122.420,"seen_pos":1},
    {"hex":"ground","lat":37.775,"lon":-122.419,"seen_pos":1,"alt_baro":"ground"},
    {"hex":"old","lat":37.775,"lon":-122.419,"seen_pos":61},
    {"hex":"far","lat":39,"lon":-121,"seen_pos":1},
    {"hex":"missing","lat":37.775,"lon":-122.419},
    {"hex":"impossible","lat":92,"lon":-122.419,"seen_pos":1}
  ]})";
  Snapshot s;assert(parse(good,s));assert(s.count==2);assert(!strcmp(s.aircraft[0].flight,"TEST1"));assert(!strcmp(s.aircraft[1].hex,"ABCD02"));assert(isnan(s.aircraft[1].altitude));assert(!strcmp(callsign(s.aircraft[1]),"ABCD02"));
  Snapshot before=s;assert(!parse(good.substr(0,good.size()-2),s));assert(s.count==before.count);assert(!parse("{}",s));assert(!parse(good+"garbage",s));
  assert(!parse(good,s,1789670100));assert(!parse(good,s,1789669000));assert(parse(good,s,1789670059));assert(s.count==1);
  assert(parse("{\"now\":1789670000000,\"ac\":[]}",s));assert(s.count==0&&s.status==Status::Live);
  std::string many="{\"now\":1789670000000,\"ac\":[";
  for(int i=39;i>=0;i--){char item[256];snprintf(item,sizeof(item),"%s{\"hex\":\"%06x\",\"lat\":%.7f,\"lon\":-122.4194,\"seen_pos\":1}",i==39?"":",",i,home.lat+i*.0001);many+=item;}
  many+="]}";assert(parse(many,s));assert(s.count==MaxAircraft);assert(!strcmp(s.aircraft[0].hex,"000000"));assert(!strcmp(s.aircraft[23].hex,"000017"));
  assert(parse(good,s));Slideshow show;show.update(s,1000);assert(show.index==0);show.advance(1,1200);show.toggleHold(1300);assert(show.held);
  std::swap(s.aircraft[0],s.aircraft[1]);show.update(s,15000);assert(show.held);assert(!strcmp(show.current()->hex,"ABCD02"));
  s.count=1;strcpy(s.aircraft[0].hex,"NEWONE");show.update(s,16000);assert(!show.held&&show.index==0);
  show.update(s,63000);assert(show.count==0);
  assert(parse(good,s,1789670000,0xfffffff0));show=Slideshow();show.update(s,0xfffffff0);show.update(s,11000);assert(show.index==1);
#ifndef WIDGET_DECK
  Touch touch;assert(touch.update(1,140,120,0)==Gesture::None);assert(touch.update(0,0,0,100)==Gesture::Tap);
  touch.update(1,200,120,200);assert(touch.update(1,160,120,400)==Gesture::Next);assert(touch.update(0,0,0,500)==Gesture::None);
  touch.update(1,140,120,600);assert(touch.update(1,140,120,1500)==Gesture::Mode);assert(touch.update(1,140,120,2000)==Gesture::None);touch.cancel();
  touch.update(1,140,120,3000);assert(touch.update(1,140,180,3200)==Gesture::Setup);touch.cancel();
  touch.update(1,140,120,4000);touch.update(-1,0,0,4010);assert(touch.update(0,0,0,4100)==Gesture::None);
  touch.update(1,140,120,0xfffffff0);assert(touch.update(0,0,0,30)==Gesture::Tap);
#endif

#ifdef WIDGET_DECK
  // Identity must come from the metadata, not generic emergency, aircraft type,
  // an embedded word, a registration suffix, or a regional codeshare assumption.
  auto identitySample=[&](const char *extra){Snapshot parsed;std::string json=std::string("{\"now\":1789670000000,\"ac\":[{\"hex\":\"abcdef\",\"lat\":37.775,\"lon\":-122.419,\"seen_pos\":1,")+extra+"}]}";assert(parse(json,parsed));assert(parsed.count==1);return parsed.aircraft[0];};
  Aircraft identity=identitySample("\"flight\":\"ual123a \",\"dbFlags\":0");
  assert(airline(identity)&&!strcmp(airline(identity)->code,"UAL"));
  for(const char *flight:{"N123UA","N911HP","XUAL123","UAL","UALABC","UAL123456","UAL12-3","SKW123","AMF123","TANKER1","CHP1"}){
    Aircraft unknown;strcpy(unknown.flight,flight);strcpy(unknown.type,"H60");assert(!airline(unknown));assert(role(unknown)==Role::Standard);
  }
  for(const char *flags:{"2","4","8","14","-1","1.5","4294967296","null","\"1\"","true"}){
    std::string extra=std::string("\"dbFlags\":")+flags;assert(role(identitySample(extra.c_str()))==Role::Standard);
  }
  assert(role(identitySample("\"dbFlags\":1"))==Role::Military);
  assert(role(identitySample("\"dbFlags\":9"))==Role::Military);
  assert(role(identitySample("\"flight\":\"CFR12\""))==Role::Fire);
  assert(role(identitySample("\"flight\":\"TRP12\""))==Role::Police);
  assert(role(identitySample("\"flight\":\"REH12\""))==Role::Rescue);
  assert(role(identitySample("\"flight\":\"CMD12\""))==Role::Rescue);
  assert(role(identitySample("\"emergency\":\"lifeguard\""))==Role::Rescue);
  assert(role(identitySample("\"emergency\":\"general\""))==Role::Standard);
  assert(role(identitySample("\"emergency\":7700"))==Role::Standard);
  assert(!airline(identitySample("\"flight\":\"UAL12\",\"dbFlags\":1")));
  assert(!airline(identitySample("\"flight\":\"UAL12\",\"emergency\":\"lifeguard\"")));
  for(const char *type:{"B06","EC45","H60","S70"}){Aircraft a;strcpy(a.type,type);assert(silhouette(a)==2);}
  for(const char *type:{"C206","GA8","BE20","C130","AT8T"}){Aircraft a;strcpy(a.type,type);assert(silhouette(a)==1);}
  Aircraft lightJet;strcpy(lightJet.type,"C510");strcpy(lightJet.category,"A1");assert(silhouette(lightJet)==0);
#endif

  std::vector<uint16_t> memory(Width*Height+2,0xdead);Canvas canvas(memory.data()+1);
  Snapshot demo;demo.count=3;demo.received=1000;demo.status=Status::Live;
  auto &a=demo.aircraft[0];strcpy(a.hex,"DEMO01");strcpy(a.flight,"SKY240");strcpy(a.type,"B738");strcpy(a.registration,"N240SK");a.altitude=12400;a.speed=286;a.distance=2.4;a.bearing=45;a.track=312;a.verticalRate=-768;
  auto &b=demo.aircraft[1];strcpy(b.hex,"DEMO02");strcpy(b.flight,"N172SK");strcpy(b.type,"C172");strcpy(b.registration,"N172SK");b.altitude=3500;b.speed=112;b.distance=4.8;b.bearing=230;b.track=82;b.verticalRate=0;
  auto &c=demo.aircraft[2];strcpy(c.hex,"DEMO03");strcpy(c.flight,"SKY HELI");strcpy(c.type,"R44");c.altitude=1250;c.speed=90;c.distance=6.1;c.bearing=140;c.track=154;c.verticalRate=320;
  show=Slideshow();show.update(demo,1000);
  auto save=[&](const char *name){assert(memory.front()==0xdead&&memory.back()==0xdead);if(argc>1){std::string path=std::string(argv[1])+"/"+name+".ppm";ppm(path.c_str(),memory.data()+1);}};
  for(int i=0;i<3;i++){show.index=i;canvas.card(show,demo,home,6000,true);char name[24];snprintf(name,sizeof(name),"aircraft-%d",i);save(name);}
#ifdef WIDGET_DECK
  Snapshot special=demo;special.count=1;
  for(int service=1;service<=4;service++)for(int kind=0;kind<3;kind++){
    special.aircraft[0]=demo.aircraft[kind];auto &plane=special.aircraft[0];
    strcpy(plane.flight,service==1?"DEMO123":service==2?"TRP123":service==3?"REH123":"CFR123");plane.dbFlags=service==1?1:0;
    assert(role(plane)==static_cast<Role>(service));
    Slideshow sample;sample.update(special,1000);canvas.card(sample,special,home,6000,true);
    char name[40];snprintf(name,sizeof(name),"role-%d-%d",service,kind);save(name);
    for(int offset:{-280,-24,24,280}){canvas.card(sample,special,home,6000,true,offset);assert(memory.front()==0xdead&&memory.back()==0xdead);}
  }
  for(const auto &brand:Airlines){
    special.aircraft[0]=demo.aircraft[0];snprintf(special.aircraft[0].flight,sizeof(special.aircraft[0].flight),"%s240",brand.code);
    assert(airline(special.aircraft[0])==&brand);
    Slideshow sample;sample.update(special,1000);canvas.card(sample,special,home,6000,true);
    std::string name=std::string("airline-")+brand.code;save(name.c_str());
  }
  puts("PASS: conservative service/airline identity, malformed metadata, type selection, 12 service variants and 18 airline badges.");
#endif
  show.index=0;show.held=true;canvas.card(show,demo,home,6000,true);save("held");
  canvas.setup("NearbySky-12AB","A1B2C3D4",home);save("setup");
  canvas.message("QUIET SKIES","NO AIRCRAFT NEARBY","WE'LL KEEP WATCHING",home,Status::Live);save("empty");
  canvas.message("FEED UNAVAILABLE","INVALID / OLD DATA","TRYING AGAIN SHORTLY",home,Status::Error);save("error");
  canvas.message("WI-FI OFFLINE","RECONNECTING TO WI-FI","SWIPE DOWN FOR SETUP",home,Status::Offline);save("offline");
  canvas.message("SYNCING TIME","SETTING THE CLOCK","SECURE CONNECTION",home,Status::Clock);save("clock");
  if(argc>2){std::string live=read(argv[2]);cJSON *root=cJSON_Parse(live.c_str());assert(root);double epoch=number(root,"now")/1000;cJSON_Delete(root);assert(parse(live,s,epoch));show=Slideshow();show.update(s,1000);if(show.count){canvas.card(show,s,home,1000);save("live-sample");}printf("Live fixture: %d fresh airborne aircraft within %.0f miles.\n",s.count,double(home.radius));}
  puts("PASS: feed validation, geography, stale/ground/radius filters, missing data, nearest-24 bound, slideshow identity/hold/expiry/rollover, gestures, native renderer guards.");
}
