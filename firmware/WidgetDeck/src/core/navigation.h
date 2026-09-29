#pragma once
#include <stdint.h>
#include <stdlib.h>
namespace deck {
enum class Event { None, Left, Right, Up, Down, Tap, Hold };
// Emit once per contact. A failed I2C sample cancels that contact.
struct Touch {
  bool down=false,used=false,moved=false;int x=0,y=0;uint32_t started=0;
  Event update(int contact,int xx,int yy,uint32_t now) {
    if(contact<0){if(down)used=true;return Event::None;}
    if(!contact){bool tap=down&&!used&&!moved&&uint32_t(now-started)<650;down=used=moved=false;return tap?Event::Tap:Event::None;}
    if(!down){down=true;used=moved=false;x=xx;y=yy;started=now;return Event::None;}
    int dx=xx-x,dy=yy-y;if(abs(dx)>12||abs(dy)>12)moved=true;
    if(!used&&uint32_t(now-started)<1200){
      if(abs(dx)>=32&&abs(dx)*10>abs(dy)*12){used=true;return dx<0?Event::Left:Event::Right;}
      if(abs(dy)>=32&&abs(dy)*10>abs(dx)*12){used=true;return dy<0?Event::Up:Event::Down;}
    }
    if(!used&&!moved&&uint32_t(now-started)>=900){used=true;return Event::Hold;}
    return Event::None;
  }
};
}
