#pragma once
#include "../../core/widget.h"
#include "fire_runtime.h"
namespace deck {
class WildfireWidget:public Widget {
 public:
  wildfire::Runtime feed;
  const char *id()const override{return "wildfire";}
  const char *title()const override{return "Nearby wildfires";}
  void begin(Services &shared)override{feed.begin(shared);}
  void tick(uint32_t now,bool active)override{feed.tick(now,active);}
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas canvas(pixels);feed.render(canvas,now);}
  void event(Event event,uint32_t now)override{if(event==Event::Up)feed.show.advance(1,now);else if(event==Event::Down)feed.show.advance(-1,now);else if(event==Event::Tap)feed.show.toggleHold(now);}
  bool command(const char *line)override{return feed.command(line);}
  uint32_t frameMs()const override{return 500;}
};
}
