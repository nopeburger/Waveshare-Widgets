#pragma once
#include "navigation.h"
#include "canvas.h"
namespace deck {
class Services;
class Widget {
 public:
  virtual ~Widget()=default;
  virtual const char *id()const=0;
  virtual const char *title()const=0;
  virtual void begin(Services &services){}
  virtual void tick(uint32_t now,bool active){}
  virtual void render(uint16_t *pixels,uint32_t now)=0;
  virtual void event(Event event,uint32_t now){}
  virtual bool command(const char *line){return false;}
  virtual uint32_t frameMs()const{return 1000;}
};
class Deck {
 public:
  Widget **widgets;const unsigned count;unsigned selected=0;
  Deck(Widget **list,unsigned size):widgets(list),count(size){}
  Widget &active(){return *widgets[selected];}
  bool select(const char *id){for(unsigned i=0;i<count;i++)if(!strcmp(widgets[i]->id(),id)){selected=i;return true;}return false;}
  // The host alone handles horizontal gestures. Vertical gestures stay local.
  bool dispatch(Event event,uint32_t now){
    if(!count)return false;
    if(event==Event::Left){selected=(selected+1)%count;return true;}
    if(event==Event::Right){selected=(selected+count-1)%count;return true;}
    if(event==Event::Up||event==Event::Down||event==Event::Tap)active().event(event,now);
    return false;
  }
  void tick(uint32_t now){for(unsigned i=0;i<count;i++)widgets[i]->tick(now,i==selected);}
  void render(uint16_t *pixels,uint32_t now){active().render(pixels,now);ui::Canvas c(pixels);c.offset=0;
    int spacing=12,start=(ui::Width-int(count)*spacing)/2;
    for(unsigned i=0;i<count;i++)c.rect(start+i*spacing,449,7,2,i==selected?ui::rgb(225,237,226):ui::rgb(42,58,61));
  }
};
}
