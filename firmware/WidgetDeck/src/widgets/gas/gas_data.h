#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace gas {
constexpr int MaxPoints=200;
struct Region {const char *code,*name,*series;};
constexpr Region Regions[]={
  {"CA","CALIFORNIA","EMM_EPMR_PTE_SCA_DPG"},
  {"CO","COLORADO","EMM_EPMR_PTE_SCO_DPG"},
  {"FL","FLORIDA","EMM_EPMR_PTE_SFL_DPG"},
  {"MA","MASSACHUSETTS","EMM_EPMR_PTE_SMA_DPG"},
  {"MN","MINNESOTA","EMM_EPMR_PTE_SMN_DPG"},
  {"NY","NEW YORK","EMM_EPMR_PTE_SNY_DPG"},
  {"OH","OHIO","EMM_EPMR_PTE_SOH_DPG"},
  {"TX","TEXAS","EMM_EPMR_PTE_STX_DPG"},
  {"WA","WASHINGTON","EMM_EPMR_PTE_SWA_DPG"},
};
constexpr bool sameCode(const char *a,const char *b){return *a==*b&&(!*a||sameCode(a+1,b+1));}
constexpr const Region *regionForCode(const char *code){
  for(const Region &region:Regions)if(sameCode(region.code,code))return &region;
  return nullptr;
}
struct Date {int year,month,day;};
inline bool leap(int y){return y%4==0&&(y%100!=0||y%400==0);}
inline int monthDays(int y,int m){static const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};return days[m-1]+(m==2&&leap(y));}
inline int dayNumber(Date d) {
  int n=0;for(int y=1970;y<d.year;y++)n+=365+leap(y);
  for(int m=1;m<d.month;m++)n+=monthDays(d.year,m);
  return n+d.day-1;
}
inline Date dateFromDay(int n) {
  Date d{1970,1,1};while(n>=365+leap(d.year)){n-=365+leap(d.year);d.year++;}
  while(n>=monthDays(d.year,d.month)){n-=monthDays(d.year,d.month);d.month++;}d.day+=n;return d;
}
inline bool parseDate(const char *s,int &out) {
  if(!s||strlen(s)!=10||s[4]!='-'||s[7]!='-')return false;
  for(int i=0;i<10;i++)if(i!=4&&i!=7&&(s[i]<'0'||s[i]>'9'))return false;
  Date d{(s[0]-'0')*1000+(s[1]-'0')*100+(s[2]-'0')*10+s[3]-'0',(s[5]-'0')*10+s[6]-'0',(s[8]-'0')*10+s[9]-'0'};
  if(d.year<2000||d.year>2100||d.month<1||d.month>12||d.day<1||d.day>monthDays(d.year,d.month))return false;
  out=dayNumber(d);return true;
}
inline int threeYearsBefore(int day){Date d=dateFromDay(day);d.year-=3;d.day=std::min(d.day,monthDays(d.year,d.month));return dayNumber(d);}
inline void dateLabel(int day,char *out,size_t size){Date d=dateFromDay(day);static const char *months[]={"JAN","FEB","MAR","APR","MAY","JUN","JUL","AUG","SEP","OCT","NOV","DEC"};snprintf(out,size,"%s %02d, %d",months[d.month-1],d.day,d.year);}
struct Point {int32_t day=0;float price=0;};
struct History {uint32_t count=0;Point points[MaxPoints];const Point &latest()const{return points[count-1];}};
inline bool normalize(History &h,int today) {
  if(!h.count||h.count>MaxPoints)return false;
  for(unsigned i=0;i<h.count;i++)if(h.points[i].day<dayNumber({2000,1,1})||h.points[i].day>dayNumber({2100,12,31})||h.points[i].day>today||!std::isfinite(h.points[i].price)||h.points[i].price<=0||h.points[i].price>100)return false;
  std::sort(h.points,h.points+h.count,[](const Point &a,const Point &b){return a.day<b.day;});
  unsigned n=0;
  for(unsigned i=0;i<h.count;i++){
    if(n&&h.points[i].day==h.points[n-1].day){if(h.points[i].price!=h.points[n-1].price)return false;continue;}
    h.points[n++]=h.points[i];
  }
  h.count=n;int cutoff=threeYearsBefore(h.latest().day);n=0;
  for(unsigned i=0;i<h.count;i++)if(h.points[i].day>=cutoff)h.points[n++]=h.points[i];
  h.count=n;return true;
}
enum class State {Loading,Current,Cached,Offline,Clock,Error,Setup,MissingKey};
struct View {History history;State state=State::Loading;int today=0;const char *regionName="CALIFORNIA";};
struct Scale {float low,high;};
inline Scale chartScale(const History &h) {
  float low=h.points[0].price,high=low;
  for(unsigned i=1;i<h.count;i++){low=std::min(low,h.points[i].price);high=std::max(high,h.points[i].price);}
  low=floorf((low-.10f)*2)/2;high=ceilf((high+.10f)*2)/2;
  return {std::max(0.f,low),std::max(high,low+.5f)};
}
}
