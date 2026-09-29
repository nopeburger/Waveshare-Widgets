#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/WidgetDeck/src/widgets/flight/sky_trail_draw.h"
#include <assert.h>
#include <vector>
#include <memory>
#include <string>
using namespace sky;
static Aircraft aircraft(float east,float north,const char *hex="ABCD01"){
  Aircraft a;snprintf(a.hex,sizeof(a.hex),"%s",hex);strcpy(a.flight,"N911TEST");strcpy(a.type,"B407");
  a.distance=hypotf(east,north);a.bearing=atan2f(east,north)*180/float(Pi);a.altitude=1250;a.speed=65;a.track=90;return a;
}
static Trail orbit(uint32_t start=1000,float turns=1,float radius=.4f,float direction=1){
  Trail trail;
  for(unsigned i=0;i<=40;i++){float t=direction*i*2*float(Pi)*turns/40;auto a=aircraft(2+radius*cosf(t),1+radius*sinf(t));a.track=90-direction*t*180/float(Pi);trail.observe(a,start+i*4000,start+i*4000);}
  return trail;
}
static Trail orbitAt(float east,float north,uint32_t start=1000,const char *hex="ABCD01"){
  Trail trail=orbit(start);
  for(unsigned i=0;i<trail.count;i++){trail.point(i).east+=east-2;trail.point(i).north+=north-1;}
  const auto &last=trail.point(trail.count-1);trail.aircraft=aircraft(last.east,last.north,hex);return trail;
}
static void airportTests(){
  Home home;home.lat=37.461102;home.lon=-122.114998;home.radius=15; // Palo Alto Airport.
  AirportZones zones;zones.configure(home);assert(zones.size()>0);
  assert(zones.contains(0,0)&&zones.contains(0,1.99f)&&!zones.contains(0,2.01f));
  assert(!zones.contains(NAN,0));
  Home sanFrancisco;sanFrancisco.lat=37.7749;sanFrancisco.lon=-122.4194;sanFrancisco.radius=15;
  AirportZones regional;regional.configure(sanFrancisco);assert(regional.size()>0);
  // SFO is inside the default search radius and has an airport buffer.
  float sfoDistance=distanceMiles(sanFrancisco,37.619806,-122.374821);
  float sfoAngle=bearingDegrees(sanFrancisco,37.619806,-122.374821)*float(Pi)/180;
  assert(sfoDistance<sanFrancisco.radius&&regional.contains(sfoDistance*sinf(sfoAngle),sfoDistance*cosf(sfoAngle)));
  Trail near=orbitAt(0,0),outside=orbitAt(0,3),edge=orbitAt(0,2.2f);
  assert(circling(near,161000)&&!circling(near,161000,&zones));
  assert(circling(outside,161000,&zones));
  assert(!zones.contains(edge.aircraft.distance*sinf(edge.aircraft.bearing*float(Pi)/180),edge.aircraft.distance*cosf(edge.aircraft.bearing*float(Pi)/180)));
  assert(circling(edge,161000)&&!circling(edge,161000,&zones));
  // A departure cannot reuse the airport loop, but a new loop outside can qualify.
  Trail departed=orbitAt(0,1.5f);departed.observe(aircraft(.4f,2.2f),165000,165000);
  assert(circling(departed,165000)&&!circling(departed,165000,&zones));
  Trail newLoop=orbitAt(0,2.6f,169000);
  for(unsigned i=0;i<newLoop.count;i++){const auto &p=newLoop.point(i);departed.observe(aircraft(p.east,p.north),p.at,p.at);}
  assert(circling(departed,329000,&zones));
  // Airport aircraft remain in the slideshow and can be tracked manually.
  auto trails=std::make_unique<Trails>();trails->configureAirports(home);
  Snapshot snapshot;snapshot.status=Status::Live;snapshot.received=161000;snapshot.count=1;snapshot.aircraft[0]=near.aircraft;
  trails->update(snapshot,161000);*trails->find("ABCD01")=near;assert(!trails->autoTrack(161000));
  Slideshow slides;slides.update(snapshot,161000);assert(slides.count==1&&slides.current());
  assert(trails->select("ABCD01")&&!trails->automatic());assert(!trails->excludeAirportTracking(161000)&&trails->tracking());
  trails->reset();assert(trails->airportCount()==zones.size());
  snapshot.count=2;snapshot.aircraft[1]=aircraft(.4f,3,"ABCD02");trails->update(snapshot,161000);
  *trails->find("ABCD01")=near;*trails->find("ABCD02")=orbitAt(0,3,1000,"ABCD02");
  assert(trails->autoTrack(161000)&&!strcmp(trails->selectedHex(),"ABCD02"));
  assert(!trails->excludeAirportTracking(161000));
  // A new position inside the buffer ends automatic tracking even before a
  // four-second trail sample is due, then leaves a 30-second slideshow interval.
  snapshot.received=163000;snapshot.aircraft[1]=aircraft(0,1.9f,"ABCD02");trails->update(snapshot,163000);
  assert(trails->excludeAirportTracking(163000)&&!trails->tracking());
  *trails->find("ABCD02")=orbitAt(0,3,33000,"ABCD02");
  assert(!trails->autoTrack(192999)&&trails->autoTrack(193000));
  Home distant;distant.lat=32.7;distant.lon=-117.2;distant.radius=15;trails->configureAirports(distant);assert(trails->airportCount()==0);
}
static void ppm(const char *path,const uint16_t *pixels){
  FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n280 456\n255\n");
  for(int i=0;i<280*456;i++){uint16_t p=pixels[i];unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);
}
int main(int argc,char **argv){
  airportTests();
  Trail circle=orbit();assert(circle.count==41&&circling(circle,161000));
  assert(circling(orbit(1000,1,.4f,-1),161000));assert(circling(orbit(1000,2),161000));
  assert(!circling(circle,180000)); // Old observations cannot trigger an automatic switch.
  assert(!circling(orbit(1000,.5f),161000)); // Ordinary broad turn, no loop.
  assert(!circling(orbit(1000,1,.015f),161000)); // Hover/position noise.
  Trail uneven;
  for(unsigned i=0;i<=40;i++){float t=i*2*float(Pi)/40,r=.4f+.055f*sinf(t*3);uint32_t now=1000+i*4000;uneven.observe(aircraft(2+r*cosf(t),1+r*sinf(t)*.8f),now,now);}
  assert(circling(uneven,161000)); // Uneven loops are detected from positions, not a circle template.
  Trail straight,zigzag,broken,teleport;
  for(unsigned i=0;i<=40;i++){
    uint32_t t=1000+i*4000;straight.observe(aircraft(i*.05f,1),t,t);
    zigzag.observe(aircraft(i*.04f,sinf(i*.7f)*.2f),t,t);
  }
  assert(!circling(straight,161000)&&!circling(zigzag,161000));
  broken=circle;for(unsigned i=20;i<broken.count;i++)broken.point(i).at+=30000;broken.lastSeen+=30000;assert(!circling(broken,191000));
  teleport=circle;teleport.point(20).east+=5;assert(!circling(teleport,161000));
  assert(circling(orbit(0xffff0000),uint32_t(0xffff0000+160000u)));
  Trail sampled;auto a=aircraft(0,1);sampled.observe(a,1000,1000);sampled.observe(a,3000,3000);assert(sampled.count==1&&sampled.lastSeen==3000);
  sampled.observe(aircraft(.01f,1),4000,4000);assert(sampled.count==1);sampled.observe(aircraft(.03f,1),7000,7000);assert(sampled.count==2);
  sampled.observe(aircraft(2,2),6000,7000);assert(sampled.lastSeen==7000&&sampled.count==2);
  sampled.prune(1000000);assert(!sampled.count);
  Trail longTrail;for(unsigned i=0;i<1000;i++){uint32_t now=1000+i*4000;longTrail.observe(aircraft(i*.001f,1),now,now);}
  assert(longTrail.count<=TrailCapacity&&longTrail.count>=220);assert(uint32_t(longTrail.lastSeen-longTrail.point(0).at)<=TrailWindow);
  auto trails=std::make_unique<Trails>();Snapshot snapshot;snapshot.status=Status::Live;snapshot.localSource=true;snapshot.received=161000;snapshot.count=1;snapshot.aircraft[0]=circle.aircraft;
  trails->update(snapshot,161000);*trails->find("ABCD01")=circle;
  assert(trails->autoTrack(161000)&&trails->automatic());assert(!trails->autoTrack(161001));
  trails->leave(161001);assert(!trails->autoTrack(161002));assert(trails->select("ABCD01")&&!trails->automatic());
  trails->leave(161002);*trails->find("ABCD01")=orbit(700000);assert(trails->autoTrack(860000));assert(trails->automatic());
  trails->reset();snapshot.received=uint32_t(0xffff0000+160000u);trails->update(snapshot,snapshot.received);*trails->find("ABCD01")=orbit(0xffff0000);
  assert(trails->autoTrack(snapshot.received));trails->leave(snapshot.received);assert(!trails->autoTrack(snapshot.received+1000));trails->select("ABCD01");
  // A pinned aircraft survives disappearing from the feed and cache churn.
  for(unsigned i=0;i<100;i++){char hex[9];snprintf(hex,sizeof(hex),"%06X",i);snapshot.aircraft[0]=aircraft(1,1,hex);snapshot.received=165000+i*4000;trails->update(snapshot,snapshot.received);}
  assert(trails->current()&&!strcmp(trails->current()->aircraft.hex,"ABCD01"));
  snapshot.count=0;snapshot.received=600000;trails->update(snapshot,600000);assert(trails->tracking());
  auto points=trails->current()->count;snapshot.status=Status::Error;snapshot.count=1;snapshot.aircraft[0]=aircraft(1,1);trails->update(snapshot,600000);assert(trails->current()->count==points);
  trails->prune(2000000);assert(trails->current()&&!trails->current()->count);trails->reset();assert(!trails->tracking());
  // Automatic tracking must expire without waiting for another feed update.
  snapshot=Snapshot();snapshot.status=Status::Live;snapshot.received=161000;snapshot.count=1;snapshot.aircraft[0]=circle.aircraft;
  trails->update(snapshot,161000);*trails->find("ABCD01")=circle;assert(trails->autoTrack(161000));
  snapshot.count=0;snapshot.received=200000;trails->update(snapshot,200000);
  assert(!trails->expireAutomatic(220999)&&trails->automatic());
  assert(trails->expireAutomatic(221000)&&!trails->tracking());assert(!trails->expireAutomatic(221001));
  // A fresh circling track cannot immediately reclaim the slideshow.
  *trails->find("ABCD01")=orbit(91000);assert(!trails->autoTrack(250999));assert(trails->autoTrack(251000));
  // Restoring observations before the deadline renews the automatic timeout.
  snapshot.count=1;snapshot.received=300000;trails->update(snapshot,300000);
  assert(!trails->expireAutomatic(359999));
  snapshot.status=Status::Error;snapshot.received=359999;trails->update(snapshot,359999);
  assert(trails->expireAutomatic(360000));
  // Cached observations in a fresh HTTP response must not extend tracking.
  trails->reset();snapshot.status=Status::Live;snapshot.received=161000;snapshot.aircraft[0]=circle.aircraft;
  trails->update(snapshot,161000);*trails->find("ABCD01")=circle;assert(trails->autoTrack(161000));
  snapshot.received=220000;snapshot.aircraft[0].age=59;trails->update(snapshot,220000);
  assert(trails->expireAutomatic(221000));
  // A manual selection, including a vertical change out of automatic mode, stays open.
  assert(trails->select("ABCD01"));assert(!trails->expireAutomatic(1000000)&&trails->tracking());
  trails->reset();snapshot.received=0xfffffff0;snapshot.aircraft[0].age=0;trails->update(snapshot,snapshot.received);
  *trails->find("ABCD01")=orbit(uint32_t(0xfffffff0-160000u));assert(trails->autoTrack(0xfffffff0));
  assert(!trails->expireAutomatic(uint32_t(0xfffffff0+59999u)));assert(trails->expireAutomatic(uint32_t(0xfffffff0+60000u)));
  // Resume a normal ten-second slide interval after leaving automatic tracking.
  Slideshow resumed;snapshot.received=1000;snapshot.count=2;snapshot.aircraft[1]=aircraft(1,1,"ABCD02");
  resumed.switched=1000;resumed.update(snapshot,1000);resumed.update(snapshot,10999);assert(resumed.index==0);
  resumed.update(snapshot,11000);assert(resumed.index==1);
  TrailBounds bounds=trailBounds(circle);assert(bounds.span<2&&bounds.span>=.5f);
  for(unsigned i=0;i<circle.count;i++){const auto &p=circle.point(i);assert(fabsf(p.east-bounds.east)<=bounds.span/2&&fabsf(p.north-bounds.north)<=bounds.span/2);}
  std::vector<uint16_t> pixels(Width*Height+2,0xdead);Canvas canvas(pixels.data()+1);snapshot.status=Status::Live;
  auto render=[&](const Trail &t,uint32_t now,const char *name,bool automatic){drawTrail(canvas,t,snapshot,now,false,automatic);assert(pixels.front()==0xdead&&pixels.back()==0xdead);if(argc>1)ppm((std::string(argv[1])+"/"+name+".ppm").c_str(),pixels.data()+1);};
  render(circle,163000,"flight-tracking",false);render(circle,163000,"flight-circling",true);render(circle,261000,"flight-tracking-lost",false);
  Trail nearby=circle;for(unsigned i=0;i<nearby.count;i++){nearby.point(i).east-=2;nearby.point(i).north-=1;}nearby.aircraft=aircraft(.4f,0);render(nearby,163000,"flight-tracking-home",false);
  Trail one;one.observe(aircraft(0,0),1000,1000);render(one,1000,"flight-tracking-start",false);
  puts("PASS: bounded trails, airport buffers/departures/re-entry, circle detection, automatic tracking timeout/recovery/cooldown/rollover, manual pinning, slideshow resumption and tracking renderer guards.");
}
