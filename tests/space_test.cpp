#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/WidgetDeck/src/widgets/space/space_feed.h"
#include "../firmware/WidgetDeck/src/widgets/space/space_draw.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
using namespace spacewx;
static std::string read(const std::string &p){std::ifstream f(p);assert(f);std::ostringstream out;out<<f.rdbuf();return out.str();}
static void ppm(const std::string &path,const uint16_t *pixels){FILE *f=fopen(path.c_str(),"wb");assert(f);fprintf(f,"P6\n280 456\n255\n");for(int i=0;i<280*456;i++){auto p=pixels[i];unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);}
int main(int argc,char **argv){
  const std::string dir="tests/fixtures/space/";int64_t epoch=std::stoll(read(dir+"captured.txt"));
  Snapshot live;std::string scales=read(dir+"scales.json"),kp=read(dir+"kp.json"),wind=read(dir+"wind.json");
  assert(parseScales(scales.data(),scales.size(),epoch,live.scales));
  assert(parseKp(kp.data(),kp.size(),epoch,live.kp));assert(parseWind(wind.data(),wind.size(),epoch,live.wind));
  for(auto &link:live.links)link=Link::Live;live.revision=1;
  assert(scalesFresh(live,epoch)&&kpFresh(live,epoch)&&windFresh(live,epoch));
  assert(live.kp.count>60&&live.kp.count<=HistoryBins&&live.kp.value>=0&&live.kp.value<=9);
  for(int i=1;i<live.kp.count;i++)assert(live.kp.samples[i].stamp>live.kp.samples[i-1].stamp);
  for(const char *invalid:{"2025-02-29T00:00:00","2026-04-31T00:00:00","2026-13-01T00:00:00","2026-09-18T24:00:00","2026-09-18T00:60:00","2026-09-18T00:00:61","2026-09-18T00:00:00-07:00","2026-09-18T00:00:00.Z","2026-09-18T00:00:00garbage","bad"})assert(!timestamp(invalid));
  assert(timestamp("2024-02-29T00:00:00Z")+86400==timestamp("2024-03-01T00:00:00"));
  assert(timestamp("2026-09-18T02:00:00.123Z")==timestamp("2026-09-18 02:00:00"));
  assert(timestamp("2026-03-08T10:00:00")-timestamp("2026-03-08T09:00:00")==3600);
  int64_t now=timestamp("2026-09-18T02:45:00");
  auto parseS=[&](const char *payload,Scales &out,int64_t at){return parseScales(payload,strlen(payload),at,out);};
  const char *sample=R"({"-1":{"G":{"Scale":"5"}},"0":{"DateStamp":"2026-09-18","TimeStamp":"02:45:00","G":{"Scale":"0"},"S":{"Scale":2},"R":{"Scale":null}},"1":{"DateStamp":"2026-09-18","TimeStamp":"02:45:00","G":{"Scale":4}},"2":{"DateStamp":"2026-09-19","TimeStamp":"00:00:00","G":{"Scale":1}}})";
  Scales data;assert(parseS(sample,data,now));assert(data.g==0&&data.s==2&&data.r==-1);assert(data.outlook[0].g==4&&data.outlook[1].g==1&&data.outlook[2].g==-1);
  assert(!parseS(sample,data,now+FreshSeconds+1)&&data.s==2);assert(!parseS(sample,data,now-121));
  for(const char *payload:{"{}","[]","{\"0\":{}}","{\"0\":","null"})assert(!parseS(payload,data,now));
  for(const char *value:{"null","true","-1","6","1.5","\"2x\"","\"\""}){
    std::string raw=std::string("{\"0\":{\"DateStamp\":\"2026-09-18\",\"TimeStamp\":\"02:45:00\",\"G\":{\"Scale\":")+value+"}}}";
    assert(parseScales(raw.data(),raw.size(),now,data)&&data.g==-1);
  }
  std::string bad=scales+"garbage";assert(!parseScales(bad.data(),bad.size(),epoch,data));
  bad=scales;bad.push_back('\0');assert(!parseScales(bad.data(),bad.size(),epoch,data));
  const char *missing=R"([{"time_tag":"2026-09-18T02:43:00","estimated_kp":7},{"time_tag":"2026-09-18T02:45:00","estimated_kp":null},{"time_tag":"2026-09-18T02:40:00","estimated_kp":"2.33"}])";
  Kp k;assert(parseKp(missing,strlen(missing),now,k)&&isnan(k.value)&&k.stamp==now&&k.count>0);
  const char *unsorted=R"([{"time_tag":"2026-09-18T02:45:00","estimated_kp":0},{"time_tag":"2026-09-18T02:40:00","estimated_kp":8},{"time_tag":"2026-09-18T02:44:00","estimated_kp":3},{"time_tag":"2026-09-18T02:44:00","estimated_kp":4},{"time_tag":"2026-09-19T00:00:00","estimated_kp":9}])";
  assert(parseKp(unsorted,strlen(unsorted),now,k)&&k.value==0&&k.count==2&&k.samples[0].value==4);
  assert(!parseKp("[]",2,now,k)&&k.value==0);assert(!parseKp(unsorted,strlen(unsorted),now+FreshSeconds+1,k));
  Wind w;const char *legacy=R"({"proton_speed":"400","time_tag":"2026-09-18T02:45:00Z"})";
  assert(parseWind(legacy,strlen(legacy),now,w)&&fabs(w.mph-894774.5168)<1);
  const char *noWind=R"([{"proton_speed":400,"time_tag":"2026-09-18T02:44:00Z"},{"proton_speed":null,"time_tag":"2026-09-18T02:45:00Z"}])";
  assert(parseWind(noWind,strlen(noWind),now,w)&&isnan(w.mph));assert(!parseWind(legacy,strlen(legacy),now+WindFreshSeconds+1,w));
  Snapshot quiet=live;quiet.scales.g=quiet.scales.s=quiet.scales.r=0;quiet.kp.value=1.33f;
  assert(!strcmp(headline(quiet,epoch),"QUIET"));
  Snapshot unknown;assert(strcmp(headline(unknown,epoch),"QUIET"));assert(!scalesFresh(unknown,epoch));
  Snapshot stale=quiet;for(auto &link:stale.links)link=Link::Error;assert(!strcmp(headline(stale,epoch),"DATA DELAYED"));
  assert(!strcmp(headline(quiet,epoch+FreshSeconds+1),"DATA DELAYED"));
  Snapshot partial=quiet;partial.scales.r=-1;assert(!strcmp(headline(partial,epoch),"PARTIAL DATA"));
  Snapshot forecast=quiet;forecast.scales.outlook[0].g=5;assert(!strcmp(headline(forecast,epoch),"QUIET"));
  Snapshot elevated=quiet;elevated.kp.value=4;assert(!strcmp(headline(elevated,epoch),"ELEVATED KP"));elevated.kp.value=5;assert(!strcmp(headline(elevated,epoch),"HIGH KP"));
  Snapshot strong=quiet;strong.scales.g=3;strong.kp.value=7.33;assert(!strcmp(headline(strong,epoch),"STRONG ACTIVITY"));
  for(int i=0;i<strong.kp.count;i++)strong.kp.samples[i].value=3.33f+4.f*i/std::max(1,strong.kp.count-1);
  Snapshot radio=quiet;radio.scales.r=5;assert(!strcmp(headline(radio,epoch),"EXTREME ACTIVITY"));assert(!strcmp(summary(radio,epoch),"RADIO BLACKOUT"));
  Snapshot solar=quiet;solar.scales.s=2;assert(!strcmp(summary(solar,epoch),"SOLAR RADIATION STORM"));
  std::vector<uint16_t> pixels(ui::Width*ui::Height+2,0xbeef);ui::Canvas canvas(pixels.data()+1);
  auto save=[&](const Snapshot &v,unsigned page,const char *name,bool demo){
    render(canvas,v,page,14000,epoch,demo);assert(pixels.front()==0xbeef&&pixels.back()==0xbeef);
    if(argc>1)ppm(std::string(argv[1])+"/"+name+".ppm",pixels.data()+1);
  };
  for(int page=0;page<3;page++){save(live,page,("space-live-"+std::to_string(page)).c_str(),false);save(strong,page,("space-storm-"+std::to_string(page)).c_str(),true);save(unknown,page,("space-empty-"+std::to_string(page)).c_str(),false);}
  save(quiet,0,"space-quiet",true);save(stale,0,"space-stale",false);save(partial,0,"space-partial",true);save(radio,0,"space-radio",true);save(solar,0,"space-radiation",true);
  for(uint32_t time:{0u,239999u,0xfffffff0u}){render(canvas,strong,0,time,epoch,true);assert(pixels.front()==0xbeef&&pixels.back()==0xbeef);}
  printf("NOAA fixture: G%d S%d R%d, Kp %.2f, %.0f mph, %d history samples.\n",live.scales.g,live.scales.s,live.scales.r,double(live.kp.value),double(live.wind.mph),live.kp.count);
  puts("PASS: NOAA schemas, UTC dates, observed/forecast separation, unknown vs zero, freshness, partial/stale failures, Kp sorting/binning/gaps, mph conversion, all pages and renderer bounds.");
}
