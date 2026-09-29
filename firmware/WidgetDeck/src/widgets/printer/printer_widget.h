#pragma once
#include "../../core/widget.h"
#include "printer_runtime.h"
namespace deck {
class PrinterWidget:public Widget {
 public:
  printer::Runtime feed;
  const char *id()const override{return "printer";}
  const char *title()const override{return "Bambu Lab P1S";}
  void begin(Services &services)override{feed.begin(services);}
  void tick(uint32_t now,bool active)override{feed.tick(now,active);}
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas c(pixels);feed.render(c,now);}
  void event(Event e,uint32_t)override{if(e==Event::Up||e==Event::Tap)feed.page=printer::nextPage(feed.page,1);else if(e==Event::Down)feed.page=printer::nextPage(feed.page,-1);}
  bool command(const char *line)override{return feed.command(line);}
  uint32_t frameMs()const override{return 1000;}
};
}
