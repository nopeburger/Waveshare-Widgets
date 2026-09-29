#pragma once
#include <vector>
#include "../firmware/WidgetDeck/src/widgets/printer/printer_camera_draw.h"
// Procedural test illustration only; never included in the firmware.
inline const std::vector<uint16_t> &cameraDemo(){
  static std::vector<uint16_t> image=[](){
    std::vector<uint16_t> pixels(ui::Width*ui::Height);ui::Canvas c(pixels.data());
    for(int y=0;y<146;y++)for(int x=0;x<260;x++)c.pixel(x,y,ui::rgb(28+y/4,30+y/4,34+y/4));
    c.rect(0,0,260,9,ui::rgb(160,161,150));c.rect(0,0,13,146,ui::rgb(20,24,29));c.rect(247,0,13,146,ui::rgb(20,24,29));
    ui::Point bed[]={{37,65},{218,65},{244,135},{15,135}};c.polygon(bed,4,ui::rgb(91,92,82));
    for(int i=0;i<8;i++)c.line(41+i*24,69,20+i*31,131,ui::rgb(105,107,95));
    for(int i=0;i<5;i++)c.line(35-i*4,77+i*12,222+i*4,77+i*12,ui::rgb(105,107,95));
    ui::Point top[]={{108,68},{145,59},{171,78},{134,87}},left[]={{108,68},{134,87},{134,118},{108,99}},right[]={{134,87},{171,78},{171,107},{134,118}};
    c.polygon(left,4,ui::rgb(127,173,132));c.polygon(right,4,ui::rgb(75,116,86));c.polygon(top,4,ui::rgb(166,210,164));
    c.rect(13,25,234,7,ui::rgb(31,33,36));c.rect(126,22,36,24,ui::rgb(15,18,21));c.line(144,46,144,57,ui::rgb(210,213,210));
    c.text(23,12,"DEMO IMAGE",ui::fontSmall,printer::Ink);
    std::vector<uint16_t> out(260*146);for(int y=0;y<146;y++)for(int x=0;x<260;x++)out[y*260+x]=pixels[y*ui::Width+x];return out;
  }();return image;
}
inline printer::CameraView cameraDemoView(){printer::CameraView v;v.pixels=cameraDemo().data();v.width=260;v.height=146;v.revision=1;v.received=0;v.state=printer::CameraState::Current;return v;}
