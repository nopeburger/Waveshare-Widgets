#pragma once
#include <math.h>
#include <algorithm>
namespace sky {
struct Home { double lat=37.7749, lon=-122.4194; float radius=15; };
inline bool validHome(const Home &h) {
  return isfinite(h.lat)&&isfinite(h.lon)&&isfinite(h.radius)&&h.lat>=-90&&h.lat<=90&&h.lon>=-180&&h.lon<=180&&h.radius>=1&&h.radius<=50;
}
inline float distanceMiles(const Home &h,double lat,double lon) {
  constexpr double pi=3.14159265358979323846;
  double a=h.lat*pi/180,b=lat*pi/180,dl=(lon-h.lon)*pi/180;
  double s=sin((b-a)/2),t=sin(dl/2),v=s*s+cos(a)*cos(b)*t*t;
  return float(3958.7613*2*asin(sqrt(std::min(1.0,std::max(0.0,v)))));
}
inline float bearingDegrees(const Home &h,double lat,double lon) {
  constexpr double pi=3.14159265358979323846;
  double a=h.lat*pi/180,b=lat*pi/180,dl=(lon-h.lon)*pi/180;
  double result=atan2(sin(dl)*cos(b),cos(a)*sin(b)-sin(a)*cos(b)*cos(dl))*180/pi;
  return float(fmod(result+360,360));
}
inline const char *compass(float bearing) {
  static const char *dirs[]={"N","NE","E","SE","S","SW","W","NW"};
  return dirs[int((bearing+22.5f)/45)&7];
}
}
