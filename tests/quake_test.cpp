#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/WidgetDeck/src/widgets/earthquake/quake_feed.h"
#include "../firmware/WidgetDeck/src/widgets/earthquake/quake_draw.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static constexpr int64_t Now=2000000000;
static cJSON *feature(const char *id,int64_t when=Now-60,double latitude=0,double longitude=0){
  cJSON *f=cJSON_CreateObject(),*p=cJSON_AddObjectToObject(f,"properties"),*g=cJSON_AddObjectToObject(f,"geometry");
  cJSON_AddStringToObject(f,"type","Feature");cJSON_AddStringToObject(f,"id",id);
  cJSON_AddStringToObject(p,"type","earthquake");cJSON_AddStringToObject(p,"status","automatic");cJSON_AddStringToObject(p,"place","3 km NNW of A Nearby Town, CA");
  cJSON_AddNumberToObject(p,"time",double(when)*1000);cJSON_AddNumberToObject(p,"updated",double(Now)*1000);cJSON_AddNumberToObject(p,"mag",1.23);
  cJSON_AddStringToObject(g,"type","Point");cJSON *coordinates=cJSON_AddArrayToObject(g,"coordinates");
  cJSON_AddItemToArray(coordinates,cJSON_CreateNumber(longitude));cJSON_AddItemToArray(coordinates,cJSON_CreateNumber(latitude));cJSON_AddItemToArray(coordinates,cJSON_CreateNumber(3.4));return f;
}
static std::string document(std::vector<cJSON*> events,int64_t generated=Now){
  cJSON *root=cJSON_CreateObject();cJSON_AddStringToObject(root,"type","FeatureCollection");cJSON *meta=cJSON_AddObjectToObject(root,"metadata");
  cJSON_AddNumberToObject(meta,"generated",double(generated)*1000);cJSON_AddNumberToObject(meta,"status",200);cJSON *list=cJSON_AddArrayToObject(root,"features");
  for(auto e:events)cJSON_AddItemToArray(list,e);char *raw=cJSON_PrintUnformatted(root);assert(raw);std::string result=raw;cJSON_free(raw);cJSON_Delete(root);return result;
}
static void ppm(const std::string &path,const uint16_t *pixels){
  FILE *f=fopen(path.c_str(),"wb");assert(f);fprintf(f,"P6\n280 456\n255\n");
  for(int i=0;i<280*456;i++){auto p=pixels[i];unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);
}
int main(int argc,char **argv){
  using namespace quake;sky::Home home;home.lat=0;home.lon=0;home.radius=1;Snapshot parsed;
  auto parse=[&](const std::string &json,int64_t now=Now){return parseFeed(json.data(),json.size(),home,now,parsed);};
  auto json=document({feature("older",Now-120),feature("newer",Now-30)});assert(parse(json));assert(parsed.count==2&&!strcmp(parsed.events[0].id,"newer"));
  assert(fabsf(parsed.events[0].magnitude-1.23f)<.001f&&fabsf(parsed.events[0].depthKm-3.4f)<.001f);
  assert(!strcmp(parsed.events[0].place,"1.9 MI NNW OF A NEARBY TOWN, CA"));
  assert(fabs(milesFromKm(1.609344)-1)<1e-10&&fabs(milesFromKm(3.4)-2.112662)<1e-6);
  assert(milesFromKm(-1.609344)==-1&&!isfinite(milesFromKm(NAN)));
  auto checkPlace=[](const char *input,const char *expected){char out[112];cJSON *item=cJSON_CreateString(input);placeText(item,out,sizeof(out));assert(!strcmp(out,expected));cJSON_Delete(item);};
  checkPlace("12.5 km SE of Town, CA","7.8 MI SE OF TOWN, CA");checkPlace("0 KM N of Town","0.0 MI N OF TOWN");
  checkPlace("  3 km NNW of Town  ","1.9 MI NNW OF TOWN");checkPlace("Northern California","NORTHERN CALIFORNIA");
  checkPlace("3 mi N of Town","3 MI N OF TOWN");checkPlace("3 kmh boundary","3 KMH BOUNDARY");checkPlace("","LOCATION NOT REPORTED");
  cJSON *smallPlace=cJSON_CreateString("3 km NNW of Town");char small[4]={'x','x','x','x'};placeText(smallPlace,small+1,2);assert(small[0]=='x'&&small[3]=='x'&&small[2]==0);placeText(smallPlace,small,0);assert(small[0]=='x');cJSON_Delete(smallPlace);
  assert(!parsed.events[0].reviewed&&sameCenter(parsed,home)&&fresh(parsed,Now)&&!fresh(parsed,Now+FreshSeconds+1));
  auto original=parsed;assert(!parse(json.substr(0,json.size()-1)));assert(parsed.count==original.count&&!strcmp(parsed.events[0].id,"newer"));
  assert(!parse(json+"junk"));assert(parse(json+"\r\n \t"));assert(!parse("{}"));assert(!parse(document({},Now-FreshSeconds-1)));assert(!parse(document({},Now+121)));
  assert(parse(document({})));assert(parsed.count==0&&parsed.status==Status::Live);

  constexpr double radiansToDegrees=180/3.14159265358979323846;
  cJSON *negative=feature("negative"),*missing=feature("missing"),*blast=feature("blast"),*deleted=feature("deleted"),*invalid=feature("invalid",Now,91,0);
  auto child=[](cJSON *item,const char *key){return cJSON_GetObjectItemCaseSensitive(item,key);};
  cJSON_ReplaceItemInObject(child(negative,"properties"),"mag",cJSON_CreateNumber(-.4));
  cJSON_ReplaceItemInObject(child(negative,"properties"),"status",cJSON_CreateString("reviewed"));
  cJSON_ReplaceItemInObject(child(missing,"properties"),"mag",cJSON_CreateNull());cJSON_ReplaceItemInObject(child(missing,"properties"),"place",cJSON_CreateNull());
  cJSON_ReplaceItemInArray(child(child(missing,"geometry"),"coordinates"),2,cJSON_CreateNull());
  cJSON_ReplaceItemInObject(child(blast,"properties"),"type",cJSON_CreateString("quarry blast"));cJSON_ReplaceItemInObject(child(deleted,"properties"),"status",cJSON_CreateString("deleted"));
  json=document({feature("inside",Now-10,99.99/3958.7613*radiansToDegrees),feature("outside",Now-10,100.01/3958.7613*radiansToDegrees),feature("expired",Now-WindowSeconds-1),feature("future",Now+121),negative,missing,blast,deleted,invalid,feature("",Now-20)});
  assert(parse(json)&&parsed.count==3);assert(!strcmp(parsed.events[0].id,"inside"));
  bool foundNegative=false,foundMissing=false;for(int i=0;i<parsed.count;i++){const auto &e=parsed.events[i];assert(e.distance<=100);if(!strcmp(e.id,"negative")){foundNegative=true;assert(e.magnitude<0&&e.reviewed);}if(!strcmp(e.id,"missing")){foundMissing=true;assert(!isfinite(e.magnitude)&&!isfinite(e.depthKm)&&strlen(e.place));}}assert(foundNegative&&foundMissing);
  home.radius=50;assert(parse(json)&&parsed.count==3); // Earthquake radius is independent of aircraft radius.
  cJSON *first=feature("revised",Now-120),*newest=feature("revised",Now-30);
  cJSON_ReplaceItemInObject(child(first,"properties"),"updated",cJSON_CreateNumber(double(Now-1)*1000));
  assert(parse(document({newest,first,feature("other",Now-60)})));assert(parsed.count==2&&!strcmp(parsed.events[0].id,"revised")&&parsed.events[0].occurred==Now-30);
  std::vector<cJSON*> many;for(int i=0;i<MaxEvents+8;i++){char id[16];snprintf(id,sizeof(id),"event%02d",i);many.push_back(feature(id,Now-1000+i));}
  assert(parse(document(many))&&parsed.count==MaxEvents&&!strcmp(parsed.events[0].id,"event39")&&!strcmp(parsed.events[31].id,"event08"));

  assert(parse(document({feature("first",Now-10),feature("second",Now-20)})));
  Slideshow show;show.update(parsed,1000,Now);assert(show.count==2&&show.index==0);show.advance(-1,2000);assert(show.index==1);show.toggleHold(2000);show.update(parsed,25000,Now);assert(show.held&&show.index==1);
  auto next=parsed;std::swap(next.events[0],next.events[1]);show.update(next,30000,Now);assert(show.held&&show.index==0&&!strcmp(show.current()->id,"second"));
  next.events[0]=next.events[1];next.count=1;show.update(next,31000,Now);assert(!show.held&&show.count==1&&!strcmp(show.current()->id,"first"));
  show.update(next,32000,Now+WindowSeconds);assert(show.count==0&&!show.held);
  show=Slideshow();show.update(parsed,0xfffffff0,Now);show.update(parsed,10000,Now);assert(show.index==1);
  char label[64];ageLabel(Now-60,Now,label,sizeof(label));assert(!strcmp(label,"1 MIN AGO"));ageLabel(Now-86400,Now,label,sizeof(label));assert(!strcmp(label,"1 DAY AGO"));ageLabel(Now+1,Now,label,sizeof(label));assert(!strcmp(label,"JUST NOW"));

  std::vector<uint16_t> memory(ui::Width*ui::Height+2,0xdead);ui::Canvas canvas(memory.data()+1);
  auto check=[&](){assert(memory.front()==0xdead&&memory.back()==0xdead);};
  for(Status status:{Status::Waiting,Status::Live,Status::Offline,Status::Clock,Status::Error}){Snapshot empty;empty.status=status;empty.generated=Now;Slideshow none;render(canvas,none,empty,1000,Now);check();}
  snprintf(parsed.events[0].place,sizeof(parsed.events[0].place),"A LONG EARTHQUAKE LOCATION DESCRIPTION THAT MUST WRAP AND END IN AN ELLIPSIS WITHOUT DRAWING OFF THE SCREEN");parsed.events[0].magnitude=11.9f;parsed.events[0].depthKm=1000;
  show=Slideshow();show.update(parsed,1000,Now);render(canvas,show,parsed,1000,Now);check();
  if(argc>2){
    std::ifstream file(argv[2]);std::ostringstream raw;raw<<file.rdbuf();json=raw.str();cJSON *root=cJSON_Parse(json.c_str());assert(root);int64_t captured=seconds(quake::get(quake::get(root,"metadata"),"generated"));cJSON_Delete(root);
    sky::Home local;assert(parseFeed(json.data(),json.size(),local,captured,parsed));assert(parsed.count>0);
    for(int i=0;i<parsed.count;i++){assert(parsed.events[i].distance<=RadiusMiles&&inWindow(parsed.events[i],captured));if(i)assert(parsed.events[i-1].occurred>=parsed.events[i].occurred);}
#ifdef _WIN32
    _putenv_s("TZ","PST8PDT");_tzset();
#endif
    show=Slideshow();show.update(parsed,1000,captured);
    for(int i=0;i<std::min(3,show.count);i++){
      show.index=i;show.held=false;render(canvas,show,parsed,1000,captured);check();ppm(std::string(argv[1])+"/earthquake-"+std::to_string(i)+".ppm",memory.data()+1);
      show.held=true;render(canvas,show,parsed,1000,captured);check();ppm(std::string(argv[1])+"/earthquake-held-"+std::to_string(i)+".ppm",memory.data()+1);
    }
    printf("Live USGS fixture: %d earthquakes in 100 miles; newest M%.1f, %.1f miles away.\n",parsed.count,double(parsed.events[0].magnitude),double(parsed.events[0].distance));
  }
  puts("PASS: USGS schema, exact radius, seven-day window, non-earthquake/deleted filters, nullable/negative magnitudes, imperial depth/place conversion, ordering/revisions/deduplication, 32-event bound, non-destructive errors, slideshow identity/hold/expiry/rollover and renderer guards.");
}
