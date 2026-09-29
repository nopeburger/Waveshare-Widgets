#ifdef NDEBUG
#undef NDEBUG
#endif
#ifdef WIDGET_DECK
#include "../firmware/WidgetDeck/src/widgets/gas/gas_feed.h"
#include "../firmware/WidgetDeck/src/widgets/gas/gas_draw.h"
#else
#include "../firmware/GasPrice/gas_feed.h"
#include "../firmware/GasPrice/gas_draw.h"
#endif
#include "../firmware/WidgetDeck/src/core/sky_settings.h"
#include <cassert>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::string read(const char *path){std::ifstream f(path);std::ostringstream s;s<<f.rdbuf();return s.str();}
static std::string row(const char *date,const char *value){return std::string("{\"period\":\"")+date+"\",\"series\":\"EMM_EPMR_PTE_SCA_DPG\",\"units\":\"$/GAL\",\"value\":"+value+"}";}
static std::string response(const std::string &rows){return "{\"response\":{\"data\":["+rows+"]}}";}
static bool parse(const std::string &json,gas::History &h){return gas::parseFeed(json.data(),json.size(),gas::dayNumber({2026,9,17}),h);}
static void ppm(const std::string &path,const uint16_t *p) {
  FILE *file=fopen(path.c_str(),"wb");assert(file);fprintf(file,"P6\n280 456\n255\n");
  for(int i=0;i<280*456;i++){unsigned char rgb[]={static_cast<unsigned char>((p[i]>>11)*255/31),static_cast<unsigned char>(((p[i]>>5)&63)*255/63),static_cast<unsigned char>((p[i]&31)*255/31)};fwrite(rgb,1,3,file);}fclose(file);
}
int main(int argc,char **argv) {
  using namespace gas;int day;
  assert(parseDate("2024-02-29",day));assert(dateFromDay(day).day==29);assert(dateFromDay(threeYearsBefore(day)).day==28);
  for(const char *bad:{"2023-02-29","2026-13-01","2026-00-01","2026-09-31","2026-9-17","2026-09-17x","1969-01-01"})assert(!parseDate(bad,day));
  for(int year=2000;year<2100;year++)for(int month=1;month<=12;month++){Date d{year,month,monthDays(year,month)},back=dateFromDay(dayNumber(d));assert(d.year==back.year&&d.month==back.month&&d.day==back.day);}
  History h;
  auto good=response(row("2026-09-14","\"5.827\"")+","+row("2026-09-07","5.678")+","+row("2026-08-31","null"));
  assert(parse(good,h)&&h.count==2&&h.points[0].day<h.latest().day&&fabsf(h.latest().price-5.827f)<.0001f);
  assert(!parse(good+"garbage",h));assert(!parse(good.substr(0,good.size()-2),h));assert(h.count==2);assert(!parse(response(""),h));
  assert(!parse(response(row("2026-09-21","5.8")),h));
  for(const char *bad:{"\"NaN\"","\"5.2x\"","\"\"","-1","0","101","true","\"Infinity\""})assert(!parse(response(row("2026-09-14",bad)),h));
  auto wrong=good;wrong.replace(wrong.find("EPMR"),4,"EPM0");assert(!parse(wrong,h));
  wrong=good;wrong.replace(wrong.find("$/GAL"),5,"OTHER");assert(!parse(wrong,h));
  assert(parse(response(row("2026-09-14","5.827")+","+row("2026-09-14","5.827")),h)&&h.count==1);
  assert(!parse(response(row("2026-09-14","5.827")+","+row("2026-09-14","5.8")),h));
  assert(parse(response(row("2026-09-14","5.827")+","+row("2023-09-14","4.5")+","+row("2023-09-13","4.5")),h)&&h.count==2);
  auto scale=chartScale(h);assert(scale.low<4.5&&scale.high>5.827);
  sky::CardSettings wifi;
  std::string settings="\xEF\xBB\xBF# existing Sky SD card\r\nssid=My home\r\npassword=abc#123=def \r\nlatitude=37.5\r\nlongitude=-121\r\nradius_miles=10\r\n";
  assert(sky::parseSettings(settings.data(),settings.size(),wifi)&&!strcmp(wifi.password,"abc#123=def "));
  for(const char *bad:{"ssid=Home\n","ssid=Home\npassword=short\n","ssid=Home\nssid=Other\npassword=abcdefgh\n","ssid=YOUR_WIFI_NAME\npassword=abcdefgh\n"})assert(!sky::parseSettings(bad,strlen(bad),wifi));
  std::vector<uint16_t> pixels(280*456+2,0xdead);Canvas canvas(pixels.data()+1);View view;view.today=dayNumber({2026,9,17});
  auto save=[&](const char *name){render(canvas,view);assert(pixels.front()==0xdead&&pixels.back()==0xdead);if(argc>1)ppm(std::string(argv[1])+"/"+name+".ppm",pixels.data()+1);};
  save("loading");view.state=State::Setup;save("setup");view.state=State::Error;save("error");view.state=State::Offline;save("offline-empty");
  if(argc>2) {
    std::string live=read(argv[2]);int today=int(std::time(nullptr)/86400);assert(parseFeed(live.data(),live.size(),today,h));assert(h.count>=150&&h.count<=158);
    assert(h.points[0].day>=threeYearsBefore(h.latest().day));
    for(unsigned i=1;i<h.count;i++)assert(h.points[i].day-h.points[i-1].day==7);
    view.today=today;view.history=h;view.state=State::Current;save("live");view.state=State::Offline;save("cached");view.today=h.latest().day+15;save("older");
    printf("Live EIA fixture: %u weekly points; latest %.3f USD/gal.\n",unsigned(h.count),double(h.latest().price));
  }
  view.history.count=1;view.history.points[0]={dayNumber({2026,9,14}),5.0f};save("single-point");
  view.history.points[0].price=12.345f;save("wide-price");
  puts("PASS: calendar/leap-year window, schema, units, series, numeric/string prices, missing values, invalid/future dates, order/deduplication, non-destructive failures, cache normalization, SD settings, framebuffer guards.");
}
