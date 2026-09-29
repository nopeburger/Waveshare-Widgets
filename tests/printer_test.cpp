#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../firmware/WidgetDeck/src/widgets/printer/printer_settings.h"
#include "../firmware/WidgetDeck/src/widgets/printer/printer_feed.h"
#include "../firmware/WidgetDeck/src/widgets/printer/printer_draw.h"
#include "printer_camera_demo.h"
#include <cassert>
#include <vector>
#include <string>
#include <ctime>
using namespace printer;
static const char *sample=R"({"print":{"command":"push_status","gcode_state":"RUNNING","mc_percent":67,"mc_remaining_time":83,"layer_num":201,"total_layer_num":300,"subtask_name":"DESK ORGANIZER","subtask_id":"101","gcode_start_time":"1789680000","nozzle_temper":219.5,"nozzle_target_temper":220,"bed_temper":55,"bed_target_temper":55,"print_error":0}})";
static void ppm(const std::string &path,const uint16_t *pixels){FILE *f=fopen(path.c_str(),"wb");assert(f);fprintf(f,"P6\n280 456\n255\n");for(int i=0;i<280*456;i++){auto p=pixels[i];unsigned char rgb[]={static_cast<unsigned char>((p>>11)*255/31),static_cast<unsigned char>(((p>>5)&63)*255/63),static_cast<unsigned char>((p&31)*255/31)};fwrite(rgb,1,3,f);}fclose(f);}
int main(int argc,char **argv){
  Settings s;std::string config="\xef\xbb\xbf# Printer\r\nip=192.168.1.101\r\nserial=01P00EXAMPLE01\r\naccess_code=A12B34C5\r\nenabled=true\r\n";
  assert(parseSettings(config.data(),config.size(),s)&&validSettings(s));
  Settings saved=s;
  for(const char *invalid:{"ip=8.8.8.8\nserial=01P00EXAMPLE01\naccess_code=A12B34C5\n","enabled=maybe","ip=192.168.1.2\nserial=01P00EXAMPLE01\naccess_code=YOURCODE\n","enabled=false\nenabled=true\n","enabled=false\nunrecognized=1\n"}){assert(!parseSettings(invalid,strlen(invalid),s));assert(!strcmp(s.accessCode,saved.accessCode));}
  assert(parseSettings("enabled=false\n",14,s)&&!s.enabled);
  assert(localAddress("10.0.0.2")&&localAddress("172.16.4.20")&&!localAddress("172.32.1.1")&&!localAddress("192.168.001.2")&&!localAddress("127.0.0.1")&&!localAddress("192.168.1.2:8883"));
  std::string nul="enabled=false\n";nul.push_back(0);assert(!parseSettings(nul.data(),nul.size(),s));
  Snapshot data;int64_t epoch=1789683600;
  auto merge=[&](const char *json,uint32_t now=1000){return mergeReport(json,strlen(json),data,now,epoch);};
  assert(merge(sample));data.link=Link::Live;assert(data.progress==67&&data.layer==201&&data.remaining==83&&fresh(data,61000)&&!fresh(data,61001));
  assert(merge(R"({"print":{"command":"push_status","mc_percent":68}})",2000));assert(data.progress==68&&data.layer==201&&data.remaining==83&&!strcmp(data.name,"DESK ORGANIZER"));
  assert(merge(R"({"print":{"command":"push_status"}})",3000)&&data.received==3000);
  assert(!merge(R"({"print":{"command":"pause","result":"success"}})"));assert(!merge(R"({"info":{"command":"get_version"}})"));
  assert(!merge("{\"print\":{}} garbage"));assert(!merge("{\"print\":"));
  assert(merge(R"({"print":{"mc_percent":null,"bed_temper":null}})"));assert(data.progress==-1&&isnan(data.bed)&&data.layer==201);
  assert(!merge(R"({"print":{"mc_percent":101,"mc_remaining_time":-2,"nozzle_temper":9999}})"));
  assert(merge(R"({"print":{"gcode_state":"PAUSE"}})"));assert(data.state==State::Paused);
  assert(merge(R"({"print":{"gcode_state":"RUNNING"}})"));assert(data.remainingEpoch==0);
  assert(merge(R"({"print":{"subtask_id":"102","mc_percent":1}})"));assert(data.progress==1&&data.remaining==-1&&data.layer==-1&&!data.name[0]);
  assert(merge(sample));assert(merge(R"({"print":{"gcode_state":"IDLE","mc_percent":100,"mc_remaining_time":83,"subtask_name":"OLD JOB"}})"));assert(data.state==State::Idle&&data.progress==-1&&data.remaining==-1&&!data.name[0]);
  assert(merge(sample));data.link=Link::Offline;assert(!fresh(data,1000));data.link=Link::Live;data.received=0xfffffff0;assert(fresh(data,30));
  char label[48];durationLabel(83,label,sizeof(label));assert(!strcmp(label,"1H 23M"));assert(fabsf(fahrenheit(55)-131)<.001f);
  char buffer[1024];Message message(buffer,sizeof(buffer));size_t n=strlen(sample),part=n/2;
  assert(!message.append(sample,int(part),0,int(n),true));assert(message.append(sample+part,int(n-part),int(part),int(n),false));assert(!strcmp(message.data,sample));
  assert(!message.append(sample,int(part),0,int(n),false));assert(!message.append(sample+part,int(n-part),int(part),int(n),false));
  assert(!message.append(sample,5,0,100000,true));assert(!message.append(sample,5,0,10,true));assert(!message.append(sample+5,5,6,10,false));
  assert(!message.append(sample,5,0,10,true));assert(!message.append(sample+5,6,5,10,false));
  std::vector<uint16_t> pixels(ui::Width*ui::Height+2,0xdead);ui::Canvas canvas(pixels.data()+1);
  assert(merge(sample));data.link=Link::Live;
  auto save=[&](const char *name,unsigned page){render(canvas,data,page,1000,epoch);assert(pixels.front()==0xdead&&pixels.back()==0xdead);if(argc>1)ppm(std::string(argv[1])+"/"+name+".ppm",pixels.data()+1);};
  save("printer-0",0);save("printer-1",1);
  uint8_t auth[80];cameraAuth(auth,"A12B34C5");assert(cameraLe32(auth)==64&&cameraLe32(auth+4)==0x3000&&cameraLe32(auth+8)==0&&cameraLe32(auth+12)==0);
  assert(!memcmp(auth+16,"bblp",4)&&!memcmp(auth+48,"A12B34C5",8));for(int i=20;i<48;i++)assert(auth[i]==0);for(int i=56;i<80;i++)assert(auth[i]==0);
  uint8_t header[16]={0x00,0x00,0x01,0x00,0,0,0,0,1,0,0,0,0,0,0,0};assert(cameraPayloadSize(header,16)==65536);
  assert(!cameraPayloadSize(header,15));header[3]=1;assert(!cameraPayloadSize(header,16));header[3]=0;header[8]=2;assert(!cameraPayloadSize(header,16));header[8]=1;header[2]=0;assert(!cameraPayloadSize(header,16));
  const uint8_t jpeg[]={0xff,0xd8,0xff,0xc0,0,11,8,2,0xd0,5,0,1,1,0x11,0,0xff,0xda,0,8,1,1,0,0,63,0,0x12,0xff,0xd9};
  int width=0,height=0;assert(cameraDimensions(jpeg,sizeof(jpeg),width,height)&&width==1280&&height==720);
  for(size_t n=0;n<sizeof(jpeg);n++)assert(!cameraDimensions(jpeg,n,width,height));
  std::vector<uint8_t> bad(jpeg,jpeg+sizeof(jpeg));bad[4]=0xff;assert(!cameraDimensions(bad.data(),bad.size(),width,height));bad.assign(jpeg,jpeg+sizeof(jpeg));bad[3]=0xc2;assert(!cameraDimensions(bad.data(),bad.size(),width,height));bad[3]=0xc0;bad[10]=1;assert(!cameraDimensions(bad.data(),bad.size(),width,height));
  bad.assign(jpeg,jpeg+sizeof(jpeg));bad[9]=8;assert(!cameraDimensions(bad.data(),bad.size(),width,height));
  bad.assign(jpeg,jpeg+sizeof(jpeg));bad[7]=8;assert(!cameraDimensions(bad.data(),bad.size(),width,height));
  bad.assign(jpeg,jpeg+sizeof(jpeg));bad[11]=4;assert(!cameraDimensions(bad.data(),bad.size(),width,height));
  std::vector<uint8_t> huge(CameraJpegLimit+1,0);huge[0]=0xff;huge[1]=0xd8;huge[huge.size()-2]=0xff;huge.back()=0xd9;assert(!cameraJpegValid(huge.data(),huge.size()));
  uint8_t output[sizeof(jpeg)]={};for(size_t chunk=1;chunk<sizeof(jpeg);chunk++){
    size_t at=0;auto reader=[&](uint8_t *p,size_t size){size_t n=std::min(chunk,std::min(size,sizeof(jpeg)-at));memcpy(p,jpeg+at,n);at+=n;return int(n);};assert(cameraReadExact(reader,output,sizeof(output))&&!memcmp(output,jpeg,sizeof(jpeg)));
  }
  assert(!cameraReadExact([](uint8_t*,size_t){return 0;},output,1));assert(!cameraReadExact([](uint8_t*,size_t){return -1;},output,1));assert(!cameraReadExact([](uint8_t*,size_t n){return int(n+1);},output,1));
  // A persistent stream can split headers and payloads anywhere or coalesce frames.
  std::vector<uint8_t> stream;
  for(unsigned frame=0;frame<5;frame++){
    uint8_t packetHeader[16]={};packetHeader[0]=sizeof(jpeg);packetHeader[8]=1;
    stream.insert(stream.end(),packetHeader,packetHeader+16);stream.insert(stream.end(),jpeg,jpeg+sizeof(jpeg));stream[stream.size()-3]=uint8_t(frame);
  }
  for(size_t chunk:{size_t(1),size_t(3),size_t(16),size_t(31),size_t(4096)}){
    std::vector<uint8_t> frameBuffer(sizeof(jpeg)+2,0xa5);CameraFrameReader reader;reader.reset(frameBuffer.data()+1,sizeof(jpeg));size_t at=0;unsigned received=0;
    while(at<stream.size()){
      size_t count=std::min(chunk,std::min(reader.remaining(),stream.size()-at));memcpy(reader.target(),stream.data()+at,count);at+=count;
      auto result=reader.advance(count);assert(result!=CameraFrameReader::Result::Invalid);
      if(result==CameraFrameReader::Result::Complete){assert(reader.size()==sizeof(jpeg)&&frameBuffer[sizeof(jpeg)-2]==received);assert(cameraDimensions(frameBuffer.data()+1,reader.size(),width,height));++received;reader.reset(frameBuffer.data()+1,sizeof(jpeg));}
      assert(frameBuffer.front()==0xa5&&frameBuffer.back()==0xa5);
    }
    assert(received==5&&reader.remaining()==16);
  }
  CameraFrameReader reader;uint8_t tiny[8]={};reader.reset(tiny,sizeof(tiny));memcpy(reader.target(),stream.data(),16);assert(reader.advance(16)==CameraFrameReader::Result::Invalid&&reader.remaining()==0);
  reader.reset(output,sizeof(output));memcpy(reader.target(),stream.data(),7);assert(reader.advance(7)==CameraFrameReader::Result::Pending);reader.reset(output,sizeof(output));assert(reader.remaining()==16);assert(reader.advance(17)==CameraFrameReader::Result::Invalid);
  reader.reset(output,sizeof(output));assert(reader.advance(0)==CameraFrameReader::Result::Invalid);
  assert(cameraFit(1280,720,width,height)&&width==260&&height==146);assert(cameraFit(640,480,width,height)&&width==260&&height==195);assert(cameraFit(8,1080,width,height)&&height==196);assert(!cameraFit(0,720,width,height)&&!cameraFit(1921,720,width,height));
  uint16_t colors[]={0xf800,0x07e0,0x001f,0xffff},resized[11];resized[0]=resized[10]=0xbeef;cameraResize(colors,2,2,resized+1,3,3);assert(resized[0]==0xbeef&&resized[10]==0xbeef&&resized[1]==colors[0]&&resized[3]==colors[1]&&resized[7]==colors[2]&&resized[9]==colors[3]);
  CameraSchedule schedule;assert(!schedule.due(false,0)&&schedule.due(true,0));schedule.started(100);assert(!schedule.due(true,5099)&&schedule.due(true,5100)&&!schedule.due(false,5100));schedule.started(0xfffffff0);assert(!schedule.due(true,4983)&&schedule.due(true,4984));
  CameraRetry retry;assert(retry.due(0));retry.failed(100);assert(retry.delayMs==5000&&!retry.due(5099)&&retry.due(5100));retry.failed(5100);assert(retry.delayMs==10000&&!retry.due(15099)&&retry.due(15100));for(int i=0;i<10;i++)retry.failed(15100);assert(retry.delayMs==60000);retry.recovered();assert(retry.due(0)&&retry.delayMs==0);retry.failed(0xfffffff0);assert(!retry.due(4983)&&retry.due(4984));
  assert(nextPage(0,-1)==2&&nextPage(2,1)==0&&nextPage(1,1)==2&&nextPage(2,-1)==1);
  CameraView camera=cameraDemoView();assert(cameraFresh(camera,15000)&&!cameraFresh(camera,15001));camera.received=0xfffffff0;assert(cameraFresh(camera,30));camera.received=0;
  auto saveCamera=[&](const char *name,uint32_t now){renderCamera(canvas,data,camera,now);assert(pixels.front()==0xdead&&pixels.back()==0xdead);if(argc>1)ppm(std::string(argv[1])+"/"+name+".ppm",pixels.data()+1);};
  saveCamera("printer-2",1000);camera.state=CameraState::Offline;assert(!cameraFresh(camera,1));saveCamera("printer-camera-stale",123000);
  camera=CameraView();saveCamera("printer-camera-waiting",1000);for(int i=0;i<=int(CameraState::Config);i++){camera.state=CameraState(i);saveCamera("printer-camera-state-check",1000);}
  data.progress=100;data.state=State::Finished;data.remaining=0;save("printer-finished",0);
  data.state=State::Paused;save("printer-paused",0);
  data.link=Link::Offline;save("printer-offline",0);
  data=Snapshot();data.link=Link::Setup;save("printer-setup",0);
  for(int i=0;i<=int(Link::Error);i++){data.link=Link(i);save("printer-state-check",0);}
  puts("PASS: printer configuration/status, camera authentication, persistent frame stream/fragment bounds, JPEG validation, scaling, five-second scheduling, retry backoff/rollover, stale snapshots, three pages, rendered states.");
}
