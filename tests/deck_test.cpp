#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/WidgetDeck/src/core/widget.h"
#include "../firmware/WidgetDeck/src/widgets/flight/sky_draw.h"
#include "../firmware/WidgetDeck/src/widgets/gas/gas_draw.h"
#include "../firmware/WidgetDeck/src/widgets/gas/gas_feed.h"
#include "../firmware/WidgetDeck/src/widgets/gas/gas_seed.h"
#include "../firmware/WidgetDeck/src/widgets/earthquake/quake_feed.h"
#include "../firmware/WidgetDeck/src/widgets/earthquake/quake_draw.h"
#include "../firmware/WidgetDeck/src/widgets/wildfire/fire_feed.h"
#include "../firmware/WidgetDeck/src/widgets/wildfire/fire_draw.h"
#include "../firmware/WidgetDeck/src/core/sky_settings.h"
#include "../firmware/WidgetDeck/src/widgets/printer/printer_draw.h"
#include "printer_camera_demo.h"
#include "../firmware/WidgetDeck/src/widgets/space/space_feed.h"
#include "../firmware/WidgetDeck/src/widgets/space/space_draw.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <vector>
#include <ctime>
struct Fake:deck::Widget {
  const char *name;deck::Event last=deck::Event::None;unsigned events=0;bool visible=false;
  Fake(const char *id):name(id){}
  const char *id()const override{return name;}
  const char *title()const override{return name;}
  void render(uint16_t *pixels,uint32_t)override{ui::Canvas c(pixels);c.clear();}
  void tick(uint32_t,bool active)override{visible=active;}
  void event(deck::Event e,uint32_t)override{last=e;events++;}
};
struct FlightPreview:Fake {
  sky::Slideshow show;sky::Snapshot data;
  FlightPreview():Fake("flight"){
    data.status=sky::Status::Live;data.count=3;data.received=1000;
    const char *ids[]={"SKY240","N172SK","SKY HELI"},*types[]={"B738","C172","R44"};
    for(int i=0;i<3;i++){auto &a=data.aircraft[i];snprintf(a.hex,sizeof(a.hex),"DEMO%d",i);strcpy(a.flight,ids[i]);strcpy(a.type,types[i]);a.altitude=i==0?12400:i==1?3500:1250;a.speed=i==0?286:i==1?112:90;a.distance=2.4f+i*1.7f;a.bearing=45+i*95;}
    show.update(data,1000);
  }
  void render(uint16_t *pixels,uint32_t now)override{sky::Canvas c(pixels);c.card(show,data,sky::Home(),now,true);}
};
struct GasPreview:Fake {
  gas::View view;
  GasPreview():Fake("gas"){}
  void render(uint16_t *pixels,uint32_t)override{ui::Canvas c(pixels);gas::render(c,view);}
};
struct EarthquakePreview:Fake {
  quake::Snapshot view;quake::Slideshow show;int64_t captured=0;
  EarthquakePreview():Fake("earthquake"){}
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas c(pixels);quake::render(c,show,view,now,captured);}
  void event(deck::Event e,uint32_t now)override{Fake::event(e,now);if(e==deck::Event::Up)show.advance(1,now);else if(e==deck::Event::Down)show.advance(-1,now);else if(e==deck::Event::Tap)show.toggleHold(now);}
};
struct WildfirePreview:Fake {
  wildfire::Snapshot view;wildfire::Slideshow show;int64_t captured=0;
  WildfirePreview():Fake("wildfire"){}
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas c(pixels);wildfire::render(c,show,view,now,captured);}
  void event(deck::Event e,uint32_t now)override{Fake::event(e,now);if(e==deck::Event::Up)show.advance(1,now);else if(e==deck::Event::Down)show.advance(-1,now);else if(e==deck::Event::Tap)show.toggleHold(now);}
};
struct PrinterPreview:Fake {
  printer::Snapshot view;unsigned page=0;
  PrinterPreview():Fake("printer"){view.link=printer::Link::Live;view.state=printer::State::Printing;view.progress=67;view.remaining=83;view.layer=201;view.totalLayers=300;view.nozzle=219.5;view.nozzleTarget=220;view.bed=view.bedTarget=55;view.received=1000;view.revision=1;view.remainingEpoch=1789683600;strcpy(view.name,"DESK ORGANIZER");}
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas c(pixels);if(page==2)printer::renderCamera(c,view,cameraDemoView(),now);else printer::render(c,view,page,now,1789683600);}
  void event(deck::Event e,uint32_t now)override{Fake::event(e,now);if(e==deck::Event::Up||e==deck::Event::Tap)page=printer::nextPage(page,1);else if(e==deck::Event::Down)page=printer::nextPage(page,-1);}
};
struct SpacePreview:Fake {
  spacewx::Snapshot view;unsigned page=0,refreshes=0;int64_t captured=0;
  SpacePreview():Fake("space"){
    auto read=[](const char *name){std::ifstream f(std::string("tests/fixtures/space/")+name);assert(f);std::ostringstream out;out<<f.rdbuf();return out.str();};
    captured=std::stoll(read("captured.txt"));std::string scales=read("scales.json"),kp=read("kp.json"),wind=read("wind.json");
    assert(spacewx::parseScales(scales.data(),scales.size(),captured,view.scales));assert(spacewx::parseKp(kp.data(),kp.size(),captured,view.kp));assert(spacewx::parseWind(wind.data(),wind.size(),captured,view.wind));for(auto &link:view.links)link=spacewx::Link::Live;
  }
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas c(pixels);spacewx::render(c,view,page,now,captured);}
  void event(deck::Event e,uint32_t now)override{Fake::event(e,now);if(e==deck::Event::Up)page=(page+1)%3;else if(e==deck::Event::Down)page=(page+2)%3;else if(e==deck::Event::Tap)++refreshes;}
};
static void ppm(const std::string &path,const uint16_t *pixels){FILE *f=fopen(path.c_str(),"wb");assert(f);fprintf(f,"P6\n280 456\n255\n");for(int i=0;i<280*456;i++){auto p=pixels[i];unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);}
int main(int argc,char **argv){
  sky::CardSettings first,second;
  const char *original="ssid=Home\npassword=abcdefgh\n",*edited="ssid=Home\npassword=abcdefgi\n";
  assert(sky::parseSettings(original,strlen(original),first));assert(sky::parseSettings(original,strlen(original),second));assert(first.fingerprint==second.fingerprint);
  assert(!sky::applyCardSettings(true,true,first.fingerprint,second.fingerprint));
  assert(sky::parseSettings(edited,strlen(edited),second));assert(first.fingerprint!=second.fingerprint);
  assert(sky::applyCardSettings(true,true,first.fingerprint,second.fingerprint));
  assert(sky::applyCardSettings(false,false,0,first.fingerprint));
  assert(!sky::applyCardSettings(true,false,0,first.fingerprint));
  auto seeded=gas::initialHistory();assert(gas::normalize(seeded,int(std::time(nullptr)/86400)));assert(seeded.count==157);
  using deck::Event;
  auto swipe=[](int dx,int dy,Event expected){deck::Touch t;assert(t.update(1,140,220,0)==Event::None);assert(t.update(1,140+dx,220+dy,100)==expected);assert(t.update(1,140+dx*2,220+dy*2,200)==Event::None);assert(t.update(0,0,0,300)==Event::None);};
  swipe(-45,3,Event::Left);swipe(45,-3,Event::Right);swipe(3,-45,Event::Up);swipe(-3,45,Event::Down);swipe(35,35,Event::None);
  deck::Touch touch;touch.update(1,140,220,0);assert(touch.update(0,0,0,100)==Event::Tap);
  touch.update(1,140,220,200);assert(touch.update(1,140,220,1100)==Event::Hold);assert(touch.update(0,0,0,1150)==Event::None);
  touch.update(1,140,220,2000);touch.update(-1,0,0,2010);assert(touch.update(1,80,220,2100)==Event::None);assert(touch.update(0,0,0,2200)==Event::None);
  touch.update(1,140,220,0xfffffff0);assert(touch.update(0,0,0,30)==Event::Tap);
  Fake a("flight"),b("gas"),c("third");deck::Widget *items[]={&a,&b,&c};deck::Deck app(items,3);
  app.dispatch(Event::Up,0);assert(a.last==Event::Up&&a.events==1&&app.selected==0);
  app.dispatch(Event::Left,0);assert(app.selected==1&&a.events==1&&b.events==0);
  app.dispatch(Event::Down,0);assert(b.last==Event::Down&&a.events==1);
  app.dispatch(Event::Left,0);app.dispatch(Event::Left,0);assert(app.selected==0);
  app.dispatch(Event::Right,0);assert(app.selected==2);app.dispatch(Event::Tap,0);assert(c.last==Event::Tap);
  app.dispatch(Event::Hold,0);assert(c.events==1);assert(!app.select("missing")&&app.selected==2);assert(app.select("gas"));app.tick(0);assert(b.visible&&!a.visible&&!c.visible);
  deck::Widget *one[]={&a};deck::Deck single(one,1);single.dispatch(Event::Right,0);assert(single.selected==0);
  if(argc>2){FlightPreview flight;GasPreview gas;std::ifstream file(argv[2]);std::ostringstream raw;raw<<file.rdbuf();std::string json=raw.str();gas.view.today=int(std::time(nullptr)/86400);assert(gas::parseFeed(json.data(),json.size(),gas.view.today,gas.view.history));gas.view.state=gas::State::Current;
    assert(seeded.count==gas.view.history.count);for(unsigned i=0;i<seeded.count;i++){assert(seeded.points[i].day==gas.view.history.points[i].day);assert(fabsf(seeded.points[i].price-gas.view.history.points[i].price)<0.0001f);}
    EarthquakePreview earthquake;
    if(argc>3){std::ifstream qfile(argv[3]);std::ostringstream qraw;qraw<<qfile.rdbuf();std::string qjson=qraw.str();cJSON *root=cJSON_Parse(qjson.c_str());assert(root);earthquake.captured=quake::seconds(quake::get(quake::get(root,"metadata"),"generated"));cJSON_Delete(root);assert(quake::parseFeed(qjson.data(),qjson.size(),sky::Home(),earthquake.captured,earthquake.view));earthquake.show.update(earthquake.view,1000,earthquake.captured);}
    WildfirePreview wildfire;
    if(argc>4){std::ifstream ffile(argv[4]);std::ostringstream fraw;fraw<<ffile.rdbuf();std::string fjson=fraw.str();wildfire.captured=argc>5?std::stoll(argv[5]):int64_t(time(nullptr));wildfire::Page page;assert(wildfire::parsePage(fjson.data(),fjson.size(),sky::Home(),wildfire.captured,wildfire.view,page)&&!page.more);wildfire.view.received=wildfire.captured;wildfire.view.status=wildfire::Status::Current;wildfire.show.update(wildfire.view,1000,wildfire.captured);}
    PrinterPreview printer;SpacePreview space;deck::Widget *widgets[]={&flight,&gas,&earthquake,&wildfire,&printer,&space};deck::Deck preview(widgets,6);std::vector<uint16_t> pixels(ui::Width*ui::Height+2,0xdead);
    auto save=[&](const char *name){preview.render(pixels.data()+1,1000);assert(pixels.front()==0xdead&&pixels.back()==0xdead);ppm(std::string(argv[1])+"/"+name+".ppm",pixels.data()+1);};
    for(int i=0;i<3;i++){flight.show.index=i;char name[24];snprintf(name,sizeof(name),"flight-%d",i);save(name);flight.show.held=true;snprintf(name,sizeof(name),"flight-held-%d",i);save(name);flight.show.held=false;}flight.show.index=0;flight.show.held=true;save("flight-held");preview.select("gas");save("gas");gas.view.state=gas::State::Offline;save("gas-cached");
    if(earthquake.show.count>1){
#ifdef _WIN32
      _putenv_s("TZ","PST8PDT");_tzset();
#endif
      preview.select("earthquake");preview.dispatch(Event::Up,1000);assert(earthquake.show.index==1);preview.dispatch(Event::Down,1000);assert(earthquake.show.index==0);preview.dispatch(Event::Tap,1000);assert(earthquake.show.held);preview.dispatch(Event::Left,1000);assert(preview.selected==3&&earthquake.show.held);preview.dispatch(Event::Left,1000);assert(preview.selected==4);preview.dispatch(Event::Left,1000);assert(preview.selected==5);preview.dispatch(Event::Left,1000);assert(preview.selected==0);preview.dispatch(Event::Right,1000);assert(preview.selected==5);preview.dispatch(Event::Right,1000);assert(preview.selected==4);preview.dispatch(Event::Right,1000);assert(preview.selected==3);preview.dispatch(Event::Right,1000);assert(preview.selected==2);
      for(int i=0;i<std::min(3,earthquake.show.count);i++){earthquake.show.index=i;earthquake.show.held=false;save(("earthquake-"+std::to_string(i)).c_str());earthquake.show.held=true;save(("earthquake-held-"+std::to_string(i)).c_str());}
      earthquake.show.held=false;earthquake.view.status=quake::Status::Offline;save("earthquake-cached");earthquake.show.count=0;save("earthquake-offline");earthquake.view.status=quake::Status::Live;save("earthquake-empty");
    }
    if(wildfire.show.count){
      preview.select("wildfire");preview.dispatch(Event::Up,1000);assert(wildfire.show.index==1%wildfire.show.count);preview.dispatch(Event::Down,1000);assert(wildfire.show.index==0);preview.dispatch(Event::Tap,1000);assert(wildfire.show.held);
      preview.dispatch(Event::Left,1000);assert(preview.selected==4&&wildfire.show.held);preview.dispatch(Event::Right,1000);assert(preview.selected==3&&wildfire.show.held);
      // Capture the nearest report and two further reports to show both unknown and reported measurements.
      for(int i=0;i<3;i++){wildfire.show.index=i?std::max(0,wildfire.show.count-3+i):0;wildfire.show.held=false;save(("wildfire-"+std::to_string(i)).c_str());wildfire.show.held=true;save(("wildfire-held-"+std::to_string(i)).c_str());}
      wildfire.show.held=false;wildfire.view.status=wildfire::Status::Offline;save("wildfire-cached");wildfire.show.count=0;save("wildfire-offline");wildfire.view.status=wildfire::Status::Current;save("wildfire-empty");
    }
    preview.select("printer");save("printer-0");preview.dispatch(Event::Up,1000);assert(printer.page==1&&preview.selected==4);save("printer-1");
    preview.dispatch(Event::Left,1000);assert(preview.selected==5);preview.dispatch(Event::Left,1000);assert(preview.selected==0);preview.dispatch(Event::Right,1000);assert(preview.selected==5);preview.dispatch(Event::Right,1000);assert(preview.selected==4&&printer.page==1);
    preview.dispatch(Event::Down,1000);assert(printer.page==0);preview.dispatch(Event::Tap,1000);assert(printer.page==1);
    preview.dispatch(Event::Up,1000);assert(printer.page==2);save("printer-2");preview.dispatch(Event::Left,1000);preview.dispatch(Event::Right,1000);assert(printer.page==2&&preview.selected==4);preview.dispatch(Event::Up,1000);assert(printer.page==0);preview.dispatch(Event::Down,1000);assert(printer.page==2);preview.dispatch(Event::Down,1000);assert(printer.page==1);
    assert(preview.select("space"));save("space-0");preview.dispatch(Event::Up,1000);assert(space.page==1&&preview.selected==5);save("space-1");preview.dispatch(Event::Up,1000);assert(space.page==2);save("space-2");
    preview.dispatch(Event::Up,1000);assert(space.page==0);preview.dispatch(Event::Down,1000);assert(space.page==2);preview.dispatch(Event::Tap,1000);assert(space.page==2&&space.refreshes==1);
    preview.dispatch(Event::Left,1000);assert(preview.selected==0);preview.dispatch(Event::Right,1000);assert(preview.selected==5&&space.page==2);assert(printer.page==1);
  }
  puts("PASS: all swipe directions, tap/hold, diagonal rejection, failed-touch cancellation, rollover, widget isolation, one/three/six-widget wrap, space page/refresh routing, stable ID selection, shared renderer bounds.");
}
