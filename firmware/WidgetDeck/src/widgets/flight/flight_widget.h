#pragma once
#include "../../core/widget.h"
#include "sky_runtime.h"
namespace deck {
class FlightWidget:public Widget {
 public:
  sky::Runtime feed;
  const char *id()const override{return "flight";}
  const char *title()const override{return "Nearby aircraft";}
  void begin(Services &shared)override{feed.begin(shared);}
  void tick(uint32_t now,bool active)override{feed.tick(now,active);}
  void render(uint16_t *pixels,uint32_t now)override{sky::Canvas canvas(pixels);feed.render(canvas,now);}
  void event(Event event,uint32_t now)override{
    if(event==Event::Up)feed.advance(1,now);
    else if(event==Event::Down)feed.advance(-1,now);
    else if(event==Event::Tap)feed.toggleTracking(now);
  }
  bool command(const char *line)override{return feed.command(line);}
  uint32_t frameMs()const override{return 50;}
};
}
