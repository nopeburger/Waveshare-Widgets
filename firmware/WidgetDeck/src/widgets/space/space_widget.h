#pragma once
#include "../../core/widget.h"
#include "space_runtime.h"
namespace deck {
class SpaceWidget:public Widget {
 public:
  spacewx::Runtime feed;
  const char *id()const override{return "space";}
  const char *title()const override{return "Space weather";}
  void begin(Services &shared)override{feed.begin(shared);}
  void tick(uint32_t now,bool active)override{feed.tick(now,active);}
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas c(pixels);feed.render(c,now);}
  void event(Event event,uint32_t now)override{
    if(event==Event::Up)feed.page=(feed.page+1)%3;
    else if(event==Event::Down)feed.page=(feed.page+2)%3;
    else if(event==Event::Tap)feed.refresh();
  }
  bool command(const char *line)override{return feed.command(line);}
  uint32_t frameMs()const override{return feed.page==0?200:1000;}
};
}
