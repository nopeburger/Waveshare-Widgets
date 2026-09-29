#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/WidgetDeck/src/widgets/flight/sky_local_feed.h"
#include "../firmware/WidgetDeck/src/widgets/flight/sky_source_settings.h"
#include "../firmware/WidgetDeck/src/widgets/flight/sky_meshpoint_protocol.h"
#include "../firmware/WidgetDeck/src/widgets/flight/sky_metadata.h"
#include <assert.h>
#include <string>
#include <fstream>
#include <sstream>
using namespace sky;
static bool local(const std::string &s,Snapshot &out,uint32_t now=1000){return parseLocalFeed(s.data(),s.size(),Home(),now,out);}
int main(int argc,char **argv){
  SourceSettings settings;
  std::string config="\xEF\xBB\xBF# Private settings\r\nsource=meshpoint\r\nip=192.168.1.100\r\nviewer_password=literal #=pass \r\ndashboard_port=8080\r\nreceiver_port=8081\r\n";
  assert(parseSourceSettings(config.data(),config.size(),settings));assert(settings.source==Source::MeshPoint);assert(!strcmp(settings.password,"literal #=pass "));
  assert(settings.dashboardPort==8080&&settings.receiverPort==8081);
  const std::string base="source=meshpoint\nip=192.168.1.100\nviewer_password=test-secret\n";
  for(const char *bad:{"","source=other\n","source=meshpoint\n","source=public\nsource=public\n","source=meshpoint\nip=8.8.8.8\nviewer_password=test\n","source=meshpoint\nip=127.0.0.1\nviewer_password=test\n","source=meshpoint\nip=192.168.068.80\nviewer_password=test\n","source=meshpoint\nip=192.168.1.100\nviewer_password=YOUR_VIEWER_PASSWORD\n"})assert(!parseSourceSettings(bad,strlen(bad),settings));
  for(const char *bad:{"dashboard_port=0\n","dashboard_port=65536\n","receiver_port=-1\n","receiver_port= 80\n","viewer_password=another\n","unknown=yes\n"}){std::string s=base+bad;assert(!parseSourceSettings(s.data(),s.size(),settings));}
  for(size_t len:{size_t(512),size_t(513)}){std::string s="source=meshpoint\nip=10.0.0.2\nviewer_password="+std::string(len,'x')+"\n";assert(parseSourceSettings(s.data(),s.size(),settings)==(len==512));}
  std::string embedded=base;embedded[embedded.size()-2]=0;assert(!parseSourceSettings(embedded.data(),embedded.size(),settings));
  assert(parseSourceSettings("source=public\n",14,settings));assert(settings.source==Source::Public&&!settings.password[0]);
  const std::string good=R"([
    {"hex":"abcd01","flight":"ual123 ","validposition":1,"lat":37.775,"lon":-122.419,"seen":2,"altitude":12300,"speed":245,"vert_rate":-512,"validtrack":1,"track":42},
    {"hex":"abcd02","validposition":1,"lat":37.780,"lon":-122.420,"seen":1,"track":42,"validtrack":0},
    {"hex":"abcd01","validposition":1,"lat":37.775,"lon":-122.419,"seen":2},
    {"hex":"badhex","validposition":1,"lat":37.775,"lon":-122.419,"seen":2},
    {"hex":"abcd03","validposition":0,"lat":37.775,"lon":-122.419,"seen":1},
    {"hex":"abcd04","validposition":1,"lat":37.775,"lon":-122.419,"seen":61},
    {"hex":"abcd05","validposition":1,"lat":39,"lon":-121,"seen":1},
    {"hex":"abcd06","validposition":1,"lat":37.775,"lon":-122.419},
    {"hex":"abcd07","validposition":1,"lat":92,"lon":-122.419,"seen":1}
  ])";
  Snapshot s;assert(local(good,s));assert(s.count==2&&s.localSource&&s.status==Status::Live);
  assert(!strcmp(s.aircraft[0].flight,"UAL123"));assert(s.aircraft[0].altitude==12300&&s.aircraft[0].speed==245&&s.aircraft[0].verticalRate==-512&&s.aircraft[0].track==42);
  assert(isnan(s.aircraft[1].altitude)&&isnan(s.aircraft[1].speed)&&isnan(s.aircraft[1].track));
  for(const std::string &bad:{good+"garbage",good.substr(0,good.size()-1),std::string("{}"),std::string("{\"detail\":\"Unauthorized\"}"),std::string("<html>login</html>")}){assert(!local(bad,s));assert(s.count==2);}
  assert(local("[]\n",s));assert(s.count==0&&s.localSource&&s.status==Status::Live);
  std::string many="[";Home home;
  for(int i=39;i>=0;i--){char item[256];snprintf(item,sizeof(item),"%s{\"hex\":\"%06x\",\"validposition\":1,\"lat\":%.7f,\"lon\":-122.4194,\"seen\":1}",i==39?"":",",i,home.lat+i*.0001);many+=item;}many+="]";
  assert(local(many,s));assert(s.count==MaxAircraft&&!strcmp(s.aircraft[0].hex,"000000")&&!strcmp(s.aircraft[23].hex,"000017"));
  assert(local(good,s));Slideshow show;show.update(s,1000);show.advance(1,1200);show.toggleHold(1200);std::swap(s.aircraft[0],s.aircraft[1]);show.update(s,1400);assert(show.held&&!strcmp(show.current()->hex,"ABCD02"));show.update(s,63000);assert(!show.count);
  Metadata metadata;Snapshot remote;remote.count=1;strcpy(remote.aircraft[0].hex,"ABCD01");strcpy(remote.aircraft[0].type,"B738");strcpy(remote.aircraft[0].registration,"N123UA");strcpy(remote.aircraft[0].flight,"WRONG99");remote.aircraft[0].dbFlags=1;remote.aircraft[0].medicalPriority=true;remote.aircraft[0].altitude=999;remote.aircraft[0].speed=999;
  metadata.update(remote,1000);assert(local(good,s));metadata.enrich(s,2000);
  assert(!strcmp(s.aircraft[0].type,"B738")&&!strcmp(s.aircraft[0].registration,"N123UA")&&s.aircraft[0].dbFlags==1);
  assert(!strcmp(s.aircraft[0].flight,"UAL123")&&s.aircraft[0].altitude==12300&&s.aircraft[0].speed==245&&!s.aircraft[0].medicalPriority&&s.aircraft[0].age==2);
  assert(!s.aircraft[1].type[0]);assert(local(good,s));metadata.enrich(s,6*60*60*1000+1001);assert(!s.aircraft[0].type[0]);
  char token[128]="unchanged";std::string login=R"({"role":"viewer","token":"abc.DEF_ghi-jkl.123"})";
  assert(meshpointToken(login.data(),login.size(),token,sizeof(token)));assert(!strcmp(token,"abc.DEF_ghi-jkl.123"));
  for(const char *bad:{"{}","{\"role\":\"admin\",\"token\":\"abc\"}","{\"role\":\"viewer\",\"token\":\"\"}","{\"role\":\"viewer\",\"token\":\"abc\\r\\nheader\"}","{\"role\":\"viewer\",\"token\":7}"})assert(!meshpointToken(bad,strlen(bad),token,sizeof(token)));
  bool running=false;assert(meshpointRunning("{\"running\":true}",16,running)&&running);assert(meshpointRunning("{\"running\":false}",17,running)&&!running);
  assert(!meshpointRunning("{\"running\":1}",13,running));assert(!meshpointRunning("{\"running\":true}bad",19,running));
  Keepalive heartbeat;assert(heartbeat.ready(100)&&!heartbeat.healthy(100));heartbeat.status(true,100);assert(heartbeat.healthy(100)&&!heartbeat.ready(30099)&&heartbeat.ready(30100));
  heartbeat.failure(30100);assert(heartbeat.healthy(30100));assert(!heartbeat.healthy(90100));heartbeat.status(false,100000);assert(!heartbeat.healthy(100000));
  heartbeat.status(true,110000);heartbeat.failure(110100,300000,true);assert(!heartbeat.healthy(110100)&&!heartbeat.ready(410099)&&heartbeat.ready(410100));
  heartbeat.status(true,0xfffffff0);assert(heartbeat.healthy(1000)&&!heartbeat.ready(1000)&&heartbeat.ready(30000));
  if(argc>1){std::ifstream f(argv[1]);std::ostringstream data;data<<f.rdbuf();assert(local(data.str(),s));printf("Captured local receiver fixture: %d nearby aircraft.\n",s.count);}
  puts("PASS: local receiver validation/units/filtering, stale expiry, identity-only enrichment, private configuration, viewer protocol and keep-alive/backoff rollover.");
}
