#pragma once
#include <stdint.h>
#include <math.h>
#include <string.h>
#include <algorithm>
#include "font.h"
namespace ui {
constexpr int Width=280,Height=456;
constexpr float Pi=3.14159265358979323846f;
inline uint16_t rgb(int r,int g,int b){return uint16_t((r>>3)<<11|(g>>2)<<5|(b>>3));}
struct Point {float x,y;};
class Canvas {
 public:
  uint16_t *pixels;int offset=0;
  explicit Canvas(uint16_t *p):pixels(p){}
  void clear(){memset(pixels,0,Width*Height*2);}
  void pixel(int x,int y,uint16_t c,int alpha=255) {
    x+=offset;if(x<0||x>=Width||y<0||y>=Height)return;
    if(alpha>=255){pixels[y*Width+x]=c;return;}
    if(alpha<=0)return;
    uint16_t old=pixels[y*Width+x];int inv=255-alpha;
    int red=((c>>11)*alpha+(old>>11)*inv)/255;
    int green=(((c>>5)&63)*alpha+((old>>5)&63)*inv)/255;
    int blue=((c&31)*alpha+(old&31)*inv)/255;
    pixels[y*Width+x]=uint16_t((red<<11)|(green<<5)|blue);
  }
  void rect(int x,int y,int w,int h,uint16_t c){for(int yy=y;yy<y+h;yy++)for(int xx=x;xx<x+w;xx++)pixel(xx,yy,c);}
  void line(float x,float y,float xx,float yy,uint16_t c,int alpha=255) {
    int n=int(fmaxf(fabsf(xx-x),fabsf(yy-y)))+1;
    for(int i=0;i<=n;i++){float t=float(i)/n,px=x+(xx-x)*t,py=y+(yy-y)*t;int ix=int(floorf(px)),iy=int(floorf(py));float fx=px-ix,fy=py-iy;
      pixel(ix,iy,c,int(alpha*(1-fx)*(1-fy)));pixel(ix+1,iy,c,int(alpha*fx*(1-fy)));
      pixel(ix,iy+1,c,int(alpha*(1-fx)*fy));pixel(ix+1,iy+1,c,int(alpha*fx*fy));}
  }
  void circle(float x,float y,float radius,uint16_t c,int alpha=255) {
    int steps=int(radius*6)+1;float px=x+radius,py=y;
    for(int i=1;i<=steps;i++){float t=float(i)*2*float(Pi)/steps,xx=x+cosf(t)*radius,yy=y+sinf(t)*radius;line(px,py,xx,yy,c,alpha);px=xx;py=yy;}
  }
  void polygon(const Point *points,int n,uint16_t color) {
    float miny=Height,maxy=0;for(int i=0;i<n;i++){miny=fminf(miny,points[i].y);maxy=fmaxf(maxy,points[i].y);}
    for(int y=std::max(0,int(floorf(miny)));y<=std::min(Height-1,int(ceilf(maxy)));y++) {
      float intersections[48];int count=0;
      for(int i=0,j=n-1;i<n;j=i++)if((points[i].y>y+.5f)!=(points[j].y>y+.5f))
        intersections[count++]=points[i].x+(y+.5f-points[i].y)*(points[j].x-points[i].x)/(points[j].y-points[i].y);
      std::sort(intersections,intersections+count);
      for(int i=0;i+1<count;i+=2)for(int x=int(ceilf(intersections[i]));x<intersections[i+1];x++)pixel(x,y,color);
    }
    for(int i=0;i<n;i++)line(points[i].x,points[i].y,points[(i+1)%n].x,points[(i+1)%n].y,color);
  }
  int textWidth(const char *s,const Font &font,int spacing=0)const {
    int w=0;for(;*s;s++){unsigned char c=*s;if(c<' '||c>126)c='?';w+=font.glyphs[c-32].advance+spacing;}return w;
  }
  void text(int x,int y,const char *s,const Font &font,uint16_t color,int spacing=0,int limit=Width-20) {
    for(;*s;s++){unsigned char c=*s;if(c<' '||c>126)c='?';const auto &g=font.glyphs[c-32];if(x+g.advance>limit)break;
      for(int yy=0;yy<g.h;yy++)for(int xx=0;xx<g.w;xx++){int i=yy*g.w+xx;unsigned char packed=font.pixels[g.offset+i/2];int a=((i&1)?packed&15:packed>>4)*17;pixel(x+xx,y+yy+g.dy,color,a);}
      x+=g.advance+spacing;}
  }
  void right(int x,int y,const char *s,const Font &f,uint16_t color,int spacing=0){text(x-textWidth(s,f,spacing),y,s,f,color,spacing,x);}
};
}
