#pragma once
#include "../../core/location.h"
namespace sky {
// Selected Bay Area airports from OurAirports public-domain data.
// Regional examples only; add airports for other regions as needed.
// Not an exhaustive airport catalog.
// Source: https://ourairports.com/data/
constexpr float AirportBufferMiles=2.f;
struct Airport {const char *id;double lat,lon;};
inline constexpr Airport LocalAirports[]={
  {"KNUQ",37.416100000,-122.049004000}, // Moffett Federal Airfield
  {"KPAO",37.461102000,-122.114998000}, // Palo Alto Airport
  {"KSJC",37.362452000,-121.929188000}, // Mineta San Jose International Airport
  {"KHWD",37.659198761,-122.122001648}, // Hayward Executive Airport
  {"KLVK",37.693401337,-121.819999695}, // Livermore Municipal Airport
  {"KRHV",37.332901000,-121.819000000}, // Reid-Hillview Airport of Santa Clara County
  {"KSQL",37.513130000,-122.250838000}, // San Carlos Airport
  {"KOAK",37.720085000,-122.221184000}, // Oakland San Francisco Bay Airport
  {"KSFO",37.619806000,-122.374821000}, // San Francisco International Airport
  {"KHAF",37.513401031,-122.500999451}, // Half Moon Bay Airport
  {"KCCR",37.989700000,-122.056999000}, // Buchanan Field
  {"KWVI",36.935699463,-121.790000916}, // Watsonville Municipal Airport
  {"CA35",38.016899109,-122.521003723}, // San Rafael Airport
  {"KAPC",38.213200000,-122.280998000}, // Napa County Airport
  {"KSUU",38.262699000,-121.927002000}, // Travis Air Force Base
  {"KDVO",38.143600464,-122.555999756}, // Marin County Airport - Gnoss Field
};
constexpr unsigned LocalAirportCount=sizeof(LocalAirports)/sizeof(LocalAirports[0]);
class AirportZones {
  struct Zone {float east,north;};Zone zones[LocalAirportCount];unsigned count=0;
 public:
  void configure(const Home &home){
    count=0;if(!validHome(home))return;
    for(const auto &airport:LocalAirports){
      float distance=distanceMiles(home,airport.lat,airport.lon);
      if(distance>home.radius+AirportBufferMiles)continue;
      float angle=bearingDegrees(home,airport.lat,airport.lon)*3.14159265358979323846f/180;
      zones[count++]={distance*sinf(angle),distance*cosf(angle)};
    }
  }
  unsigned size()const{return count;}
  bool contains(float east,float north)const{
    if(!isfinite(east)||!isfinite(north))return false;
    for(unsigned i=0;i<count;i++){float dx=east-zones[i].east,dy=north-zones[i].north;if(dx*dx+dy*dy<=AirportBufferMiles*AirportBufferMiles)return true;}
    return false;
  }
};
}
