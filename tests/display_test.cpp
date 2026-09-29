#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/WidgetDeck/src/core/display_policy.h"
#include <cassert>
#include <cstdio>

int main(){
  using deck::DisplayPolicy;
  DisplayPolicy p;
  // Out of the box, night mode is off and brightness is fixed even without time.
  assert(!p.settings.nightEnabled);
  for(int minute=0;minute<1440;minute++){p.tick(0,minute);assert(p.percent==85&&!p.night);}
  p.tick(0,-1);assert(p.percent==85&&!p.clockKnown);
  p.touch(10);p.tick(10,1380);assert(p.percent==85);
  p.clearTouch();p.settings.nightEnabled=true;
  // Exhaust the complete local day, including both exact boundary minutes.
  for(int minute=0;minute<1440;minute++){
    p.tick(0,minute);
    assert(p.clockKnown);
    assert(p.percent==((minute<420||minute>=1350)?20:85));
  }
  assert(DisplayPolicy::panelValue(20)==51);
  assert(DisplayPolicy::panelValue(85)==217);
  assert(DisplayPolicy::panelValue(100)==255);
  // Midnight does not interrupt a touch override; the timeout is monotonic.
  p.touch(1000);p.tick(1000,1439);assert(p.percent==100);
  p.tick(30999,0);assert(p.percent==100);
  p.tick(31000,0);assert(p.percent==20&&!p.awake);
  // Contact keeps extending the override even without a new tap/swipe event.
  p.touch(40000);p.tick(40000,1350);assert(p.percent==100);
  p.touch(69000);p.tick(70000,1350);assert(p.percent==100);
  p.tick(98999,1350);assert(p.percent==100);
  p.tick(99000,1350);assert(p.percent==20);
  // Daytime contact stays at 85%; a recent contact carries into night mode.
  p.touch(100000);p.tick(100000,1349);assert(p.percent==85&&!p.night);
  p.tick(105000,1350);assert(p.percent==100&&p.night);
  p.tick(130000,1350);assert(p.percent==20);
  p.tick(131000,420);assert(p.percent==85&&!p.night);
  // Unknown time uses the daytime level until synchronization.
  p.tick(132000,-1);assert(!p.clockKnown&&p.percent==85);
  p.touch(133000);p.tick(133000,-1);assert(p.percent==85);
  p.tick(163000,1440);assert(!p.clockKnown&&p.percent==85);
  p.tick(164000,720);assert(p.clockKnown&&p.percent==85);
  // At 07:00 the daytime level wins even while a night touch boost is active.
  p.touch(170000);p.tick(170000,419);assert(p.percent==100);
  p.tick(171000,420);assert(p.percent==85&&!p.night);
  // NTP corrections / DST repeats do not alter the 30-second elapsed timeout.
  p.touch(200000);p.tick(200000,119);assert(p.percent==100);
  p.tick(215000,60);assert(p.percent==100);
  p.tick(230000,60);assert(p.percent==20);
  p.tick(230001,180);assert(p.percent==20);
  // Both expiry and retirement stay correct across uint32_t millis rollover.
  p.touch(0xfffffff0u);p.tick(0xfffffff0u,60);assert(p.percent==100);
  p.tick(29983,60);assert(p.percent==100);
  p.tick(29984,60);assert(p.percent==20&&!p.awake);
  p.tick(0xfffffff0u,60);assert(p.percent==20);
  p.touch(0);p.tick(0,60);assert(p.percent==100);
  p.clearTouch();p.tick(0,60);assert(p.percent==20);
  DisplayPolicy custom;
  custom.settings.nightEnabled=true;
  custom.settings.dayPercent=70;custom.settings.nightPercent=12;custom.settings.touchPercent=90;
  custom.settings.nightStart=20*60;custom.settings.nightEnd=6*60;custom.settings.touchTimeout=5000;
  custom.tick(0,19*60);assert(custom.percent==70);
  custom.tick(0,20*60);assert(custom.percent==12);
  custom.touch(100);custom.tick(100,21*60);assert(custom.percent==90);
  custom.tick(5100,21*60);assert(custom.percent==12&&!custom.awake);
  custom.tick(6000,6*60);assert(custom.percent==70);
  custom.settings.nightStart=60;custom.settings.nightEnd=180;
  custom.tick(7000,120);assert(custom.percent==12);
  custom.tick(8000,23*60);assert(custom.percent==70);
  puts("PASS: default-off and custom brightness, schedule boundaries, touch timeout, clock corrections, DST repeat, millis rollover.");
}
