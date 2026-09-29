#pragma once
#include "../../core/widget.h"
#include "quake_runtime.h"
namespace deck {
class EarthquakeWidget:public Widget {
 public:
  quake::Runtime feed;
  const char *id()const override{return "earthquake";}
  const char *title()const override{return "Recent earthquakes";}
  void begin(Services &shared)override{feed.begin(shared);}
  void tick(uint32_t now,bool active)override{feed.tick(now,active);}
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas canvas(pixels);feed.render(canvas,now);}
  void event(Event event,uint32_t now)override{if(event==Event::Up)feed.show.advance(1,now);else if(event==Event::Down)feed.show.advance(-1,now);else if(event==Event::Tap)feed.show.toggleHold(now);}
  bool command(const char *line)override{return feed.command(line);}
  uint32_t frameMs()const override{return 500;}
};
}
