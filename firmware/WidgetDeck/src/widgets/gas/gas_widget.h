#pragma once
#include "../../core/widget.h"
#include "gas_runtime.h"
#include "gas_draw.h"
namespace deck {
class GasWidget:public Widget {
 public:
  gas::Runtime feed;
  const char *id()const override{return "gas";}
  const char *title()const override{return "Gas Prices";}
  void begin(Services &shared)override{feed.begin(shared);}
  void render(uint16_t *pixels,uint32_t now)override{ui::Canvas canvas(pixels);gas::render(canvas,feed.snapshot());}
  void event(Event event,uint32_t now)override{if(event==Event::Tap)feed.refresh();}
  bool command(const char *line)override{
    if(!strcmp(line,"gas refresh")){feed.refresh();return true;}
    if(!strcmp(line,"gas status")){auto v=feed.snapshot();Serial.printf("gas points=%u state=%d latest=%.3f\n",unsigned(v.history.count),int(v.state),v.history.count?double(v.history.latest().price):0.0);return true;}
    return false;
  }
};
}
