#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/WidgetDeck/src/widgets/wildfire/fire_feed.h"
#include "../firmware/WidgetDeck/src/widgets/wildfire/fire_draw.h"
#include <cassert>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <ctime>
static constexpr int64_t Now=2000000000;
static cJSON *child(cJSON *j,const char *key){return cJSON_GetObjectItemCaseSensitive(j,key);}
static cJSON *feature(const char *id,double miles=10){
  cJSON *f=cJSON_CreateObject(),*a=cJSON_AddObjectToObject(f,"attributes"),*g=cJSON_AddObjectToObject(f,"geometry");
  cJSON_AddStringToObject(a,"IrwinID",id);cJSON_AddStringToObject(a,"IncidentName","Nearby Fire");cJSON_AddStringToObject(a,"POOCounty","Test County");cJSON_AddStringToObject(a,"POOState","US-CA");
  cJSON_AddStringToObject(a,"IncidentTypeCategory","WF");cJSON_AddNumberToObject(a,"ActiveFireCandidate",1);cJSON_AddNullToObject(a,"FireOutDateTime");
  cJSON_AddNumberToObject(a,"ModifiedOnDateTime_dt",double(Now-60)*1000);cJSON_AddNumberToObject(a,"FireDiscoveryDateTime",double(Now-90*86400)*1000);
  cJSON_AddNumberToObject(a,"IncidentSize",1234);cJSON_AddNumberToObject(a,"PercentContained",45);
  cJSON_AddNumberToObject(g,"x",0);cJSON_AddNumberToObject(g,"y",miles/3958.7613*180/3.14159265358979323846);return f;
}
static std::string document(std::vector<cJSON*> items,bool more=false,int sr=4326){
  cJSON *root=cJSON_CreateObject();cJSON_AddStringToObject(root,"geometryType","esriGeometryPoint");cJSON_AddNumberToObject(cJSON_AddObjectToObject(root,"spatialReference"),"wkid",sr);cJSON_AddBoolToObject(root,"exceededTransferLimit",more);
  cJSON *list=cJSON_AddArrayToObject(root,"features");for(auto f:items)cJSON_AddItemToArray(list,f);char *raw=cJSON_PrintUnformatted(root);assert(raw);std::string result=raw;cJSON_free(raw);cJSON_Delete(root);return result;
}
int main(int argc,char **argv){
  using namespace wildfire;sky::Home home;home.lat=0;home.lon=0;home.radius=1;Snapshot parsed;Page info;
  auto parse=[&](const std::string &json){return parsePage(json.data(),json.size(),home,Now,parsed,info);};
  auto json=document({feature("near",1),feature("far",50)});assert(parse(json)&&parsed.count==2&&info.rows==2&&!info.more);
  assert(!strcmp(parsed.reports[0].id,"NEAR")&&!strcmp(parsed.reports[0].name,"NEARBY FIRE")&&parsed.reports[0].acres==1234&&parsed.reports[0].contained==45);
  assert(parsed.reports[0].discovered<Now-7*86400); // Long-running current incidents remain eligible.
  assert(!parse(json.substr(0,json.size()-1))&&parsed.count==2);assert(!parse(json+"junk"));assert(!parse("{}"));assert(!parse("{\"error\":{\"code\":400}}"));assert(!parse(document({},false,3857)));assert(!parse(document({},true)));
  assert(parsed.count==2&&!strcmp(parsed.reports[0].id,"NEAR"));assert(parse(json+"\r\n"));
  auto closed=feature("closed"),rx=feature("prescribed"),inactive=feature("inactive"),otherState=feature("nv"),future=feature("future"),badGeo=feature("bad"),missing=feature("missing"),contained=feature("contained");
  cJSON_ReplaceItemInObject(child(closed,"attributes"),"FireOutDateTime",cJSON_CreateNumber(double(Now-20)*1000));
  cJSON_ReplaceItemInObject(child(rx,"attributes"),"IncidentTypeCategory",cJSON_CreateString("RX"));
  cJSON_ReplaceItemInObject(child(inactive,"attributes"),"ActiveFireCandidate",cJSON_CreateNumber(0));
  cJSON_ReplaceItemInObject(child(otherState,"attributes"),"POOState",cJSON_CreateString("US-NV"));
  cJSON_ReplaceItemInObject(child(future,"attributes"),"ModifiedOnDateTime_dt",cJSON_CreateNumber(double(Now+121)*1000));
  cJSON_ReplaceItemInObject(child(badGeo,"geometry"),"x",cJSON_CreateNumber(181));
  for(const char *key:{"IncidentSize","PercentContained","IncidentName","POOCounty","ModifiedOnDateTime_dt"})cJSON_ReplaceItemInObject(child(missing,"attributes"),key,cJSON_CreateNull());
  cJSON_ReplaceItemInObject(child(contained,"attributes"),"PercentContained",cJSON_CreateNumber(100));
  parsed=Snapshot();json=document({closed,rx,inactive,otherState,future,badGeo,missing,contained,feature("inside",99.99),feature("outside",100.01),feature(""),feature("bad id")});
  assert(parse(json)&&parsed.count==3);bool foundMissing=false,foundContained=false;
  for(int i=0;i<parsed.count;i++){auto &r=parsed.reports[i];assert(r.distance<=100);if(!strcmp(r.id,"MISSING")){foundMissing=true;assert(!isfinite(r.acres)&&!isfinite(r.contained)&&!r.updated&&!strcmp(r.name,"UNNAMED FIRE"));}if(!strcmp(r.id,"CONTAINED")){foundContained=true;assert(r.contained==100);}}assert(foundMissing&&foundContained);
  home.radius=50;parsed=Snapshot();assert(parse(json)&&parsed.count==3);
  auto invalidNumbers=feature("numbers");cJSON_ReplaceItemInObject(child(invalidNumbers,"attributes"),"IncidentSize",cJSON_CreateNumber(-1));cJSON_ReplaceItemInObject(child(invalidNumbers,"attributes"),"PercentContained",cJSON_CreateNumber(101));parsed=Snapshot();assert(parse(document({invalidNumbers})));assert(!isfinite(parsed.reports[0].acres)&&!isfinite(parsed.reports[0].contained));
  parsed=Snapshot();assert(parse(document({feature("revised",10)},true))&&info.more);auto newer=feature("revised",8);cJSON_ReplaceItemInObject(child(newer,"attributes"),"ModifiedOnDateTime_dt",cJSON_CreateNumber(double(Now-1)*1000));assert(parse(document({newer,feature("second",5)})));assert(parsed.count==2&&!strcmp(parsed.reports[0].id,"SECOND")&&parsed.reports[1].distance<9);
  assert(parse(document({feature("revised",11)})));assert(parsed.count==2&&parsed.reports[1].distance<9);
  std::vector<cJSON*> many;for(int i=0;i<MaxReports+8;i++){char id[20];snprintf(id,sizeof(id),"fire%02d",i);many.push_back(feature(id,i+1));}parsed=Snapshot();assert(parse(document(many))&&parsed.count==MaxReports&&parsed.reports[MaxReports-1].distance<17);
  parsed=Snapshot();assert(parse(document({}))&&parsed.count==0);parsed.received=Now;parsed.status=Status::Current;parsed.lat=home.lat;parsed.lon=home.lon;assert(sameCenter(parsed,home)&&fresh(parsed,Now)&&!fresh(parsed,Now+FreshSeconds+1)&&usable(parsed,Now+FreshSeconds+1));
  assert(parse(document({feature("a",1),feature("b",2)})));Slideshow show;show.update(parsed,1000,Now);show.advance(-1,2000);assert(show.index==1);show.toggleHold(2000);show.update(parsed,25000,Now);assert(show.held&&show.index==1);
  std::swap(parsed.reports[0],parsed.reports[1]);show.update(parsed,26000,Now);assert(show.held&&show.index==0&&!strcmp(show.current()->id,"B"));parsed.reports[0]=parsed.reports[1];parsed.count=1;show.update(parsed,27000,Now);assert(!show.held&&show.count==1);
  show.update(parsed,28000,Now+CacheSeconds+1);assert(!show.count&&!show.held);parsed.count=2;strcpy(parsed.reports[1].id,"OTHER");show=Slideshow();show.update(parsed,0xfffffff0,Now);show.update(parsed,10000,Now);assert(show.index==1);
  char label[96];acresLabel(.01f,label,sizeof(label));assert(!strcmp(label,"<0.1"));acresLabel(25435,label,sizeof(label));assert(!strcmp(label,"25,435"));acresLabel(NAN,label,sizeof(label));assert(!strcmp(label,"--"));updatedLabel(0,Now,label,sizeof(label));assert(!strcmp(label,"UPDATE UNKNOWN"));updatedLabel(Now-86400,Now,label,sizeof(label));assert(!strcmp(label,"UPDATED 1 DAY AGO"));
  char url[1024];assert(queryUrl(0,url,sizeof(url))&&strstr(url,"US-CA")&&!strstr(url,"geometry=")&&!strstr(url,"latitude")&&!strstr(url,"longitude"));assert(queryUrl(1,url,sizeof(url))&&strstr(url,"resultOffset=128"));assert(!queryUrl(MaxPages,url,sizeof(url)));assert(!queryUrl(0,url,10));
  std::vector<uint16_t> memory(ui::Width*ui::Height+2,0xdead);ui::Canvas canvas(memory.data()+1);auto check=[&](){assert(memory.front()==0xdead&&memory.back()==0xdead);};
  for(Status status:{Status::Waiting,Status::Current,Status::Offline,Status::Clock,Status::Error}){Snapshot empty;empty.status=status;empty.received=Now;Slideshow none;render(canvas,none,empty,1000,Now);check();}
  strcpy(parsed.reports[0].name,"A VERY LONG INCIDENT NAME THAT MUST END IN AN ELLIPSIS");strcpy(parsed.reports[0].county,"A VERY LONG COUNTY NAME TO FIT ON THE DISPLAY");parsed.reports[0].acres=999999;parsed.reports[0].contained=100;parsed.count=1;show=Slideshow();show.update(parsed,1000,Now);render(canvas,show,parsed,1000,Now);check();
  if(argc>1){std::ifstream file(argv[1]);std::ostringstream raw;raw<<file.rdbuf();json=raw.str();int64_t captured=argc>2?std::stoll(argv[2]):int64_t(time(nullptr));sky::Home local;parsed=Snapshot();assert(parsePage(json.data(),json.size(),local,captured,parsed,info)&&!info.more);assert(parsed.count>0);for(int i=0;i<parsed.count;i++){assert(parsed.reports[i].distance<=100);if(i)assert(parsed.reports[i-1].distance<=parsed.reports[i].distance);}printf("NIFC fixture: %d nearby reports from %d California records; nearest %s at %.1f miles.\n",parsed.count,info.rows,parsed.reports[0].name,double(parsed.reports[0].distance));}
  puts("PASS: NIFC schema/projection, local 100-mile filtering, current California wildfires only, unknown fields, containment, revisions, pagination accumulation, nearest-16 bound, cache expiry, slideshow identity/hold/rollover, coordinate-free query and renderer guards.");
}
