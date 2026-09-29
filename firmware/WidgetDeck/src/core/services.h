#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <HTTPClient.h>
#include <NetworkClientSecure.h>
#include <WebServer.h>
#include <Preferences.h>
#include <time.h>
#include "esp_heap_caps.h"
#include "esp_random.h"
#include "mbedtls/platform.h"
#include "cJSON.h"
#include "canvas.h"
#include "sky_sd.h"

namespace deck {
using sky::Home;using sky::validHome;using sky::CardSettings;using sky::loadCardSettings;
class Services {
 public:
  Home home;bool portal=false,setupVisible=false;char apName[32]={},apPassword[12]={};
  SemaphoreHandle_t networkMutex=nullptr;
  Home location(uint32_t &revision){xSemaphoreTake(mutex,portMAX_DELAY);Home h=networkHome;revision=configRevision;xSemaphoreGive(mutex);return h;}
  void toggleSetup(){if(setupVisible){setupVisible=false;return;}openPortal();}
  void renderSetup(uint16_t *pixels){
    ui::Canvas c(pixels);auto ink=ui::rgb(255,255,255),accent=ui::rgb(255,188,105);
    c.clear();c.text(20,20,"WIDGET DECK",ui::fontHeading,ink);c.text(20,70,"LET'S CONNECT",ui::fontNumber,ink);
    c.text(20,122,"1 / JOIN THIS WI-FI",ui::fontSmall,accent);c.text(20,149,apName,ui::fontBody,ink);
    c.text(20,190,"PASSWORD",ui::fontSmall,ink);c.text(20,215,apPassword,ui::fontNumber,ink);
    c.text(20,275,"2 / OPEN IN YOUR BROWSER",ui::fontSmall,accent);c.text(20,303,"192.168.4.1",ui::fontNumber,ink);
    c.text(20,360,"ADD YOUR HOME WI-FI",ui::fontBody,ink);c.text(20,422,"HOLD TO RETURN TO WIDGETS",ui::fontSmall,ink);
  }
  void begin(const char *fallbackSsid,const char *fallbackPassword) {
    // Configure once, before Wi-Fi/TLS workers start. The persistent printer TLS
    // session must leave internal RAM available for Wi-Fi, task stacks and DMA.
    int tlsConfigured=mbedtls_platform_set_calloc_free(tlsAllocate,free);
    assert(tlsConfigured==0);(void)tlsConfigured;
    mutex=xSemaphoreCreateMutex();networkMutex=xSemaphoreCreateMutex();assert(mutex&&networkMutex);config.begin("nearby-sky",false);
    home.lat=config.getDouble("lat",home.lat);home.lon=config.getDouble("lon",home.lon);home.radius=config.getFloat("radius",home.radius);
    if(!validHome(home))home=Home();
    ssid=config.getString("ssid",fallbackSsid);password=config.getString("pass",fallbackPassword);
    CardSettings card;card.home=home;
    if(loadCardSettings(card,sdState)){
      bool saved=config.isKey("ssid")&&ssid.length();
      bool tracked=config.isKey("sd-hash");uint64_t previous=config.getULong64("sd-hash",0);
      if(sky::applyCardSettings(saved,tracked,previous,card.fingerprint)){
        ssid=card.ssid;password=card.password;home=card.home;
        config.putString("ssid",ssid);config.putString("pass",password);config.putDouble("lat",home.lat);config.putDouble("lon",home.lon);config.putFloat("radius",home.radius);
        Serial.println("SD: applied wifi.txt settings.");
      }else Serial.println("SD: retaining saved setup; edit wifi.txt to replace it.");
      if(!tracked||previous!=card.fingerprint)config.putULong64("sd-hash",card.fingerprint);
      memset(&card,0,sizeof(card));
    }
    networkHome=home;
    snprintf(apName,sizeof(apName),"WidgetDeck-%04X",unsigned(ESP.getEfuseMac()&0xffff));
    snprintf(apPassword,sizeof(apPassword),"%08X",unsigned(esp_random()));
    snprintf(token,sizeof(token),"%08X%08X",unsigned(esp_random()),unsigned(esp_random()));
    setupRoutes();
    // Configure cJSON once before any widget worker starts. All trees use PSRAM.
    cJSON_Hooks hooks={jsonAllocate,free};cJSON_InitHooks(&hooks);
    WiFi.onEvent([this](arduino_event_id_t event,arduino_event_info_t info){
      if(event==ARDUINO_EVENT_WIFI_STA_DISCONNECTED){disconnectReason=info.wifi_sta_disconnected.reason;Serial.printf("Wi-Fi: disconnected, reason %u (%s).\n",unsigned(disconnectReason),WiFi.STA.disconnectReasonName(static_cast<wifi_err_reason_t>(disconnectReason)));}
      else if(event==ARDUINO_EVENT_WIFI_STA_GOT_IP){disconnectReason=0;Serial.println("Wi-Fi: connected; synchronizing time and updating widgets.");}
    });
    if(ssid.length())connect();else openPortal();

  }
  void tick(uint32_t now) {
    if(portal)server.handleClient();
    if(scanAt&&int32_t(now-scanAt)>=0){scanAt=0;WiFi.scanNetworks(true,true);scanning=true;}
    if(scanning&&WiFi.scanComplete()!=WIFI_SCAN_RUNNING) {
      int count=WiFi.scanComplete(),matches=0;
      for(int i=0;i<count;i++)if(WiFi.SSID(i)==ssid){++matches;Serial.printf("Configured Wi-Fi found: signal=%d dBm channel=%d security=%d.\n",int(WiFi.RSSI(i)),int(WiFi.channel(i)),int(WiFi.encryptionType(i)));}
      if(count<0)Serial.println("Wi-Fi scan failed; reconnecting.");else Serial.printf("Wi-Fi scan complete: %d visible networks, %d matches for configured name.\n",count,matches);
      WiFi.scanDelete();scanning=false;connect();
    }
    if(applyAt&&int32_t(now-applyAt)>=0){applyAt=0;connect();}
    if(WiFi.status()==WL_CONNECTED) {
      everConnected=true;
      if(portal&&applyCompleteAt==0&&closingPortal)applyCompleteAt=now+15000;
      if(applyCompleteAt&&int32_t(now-applyCompleteAt)>=0){server.stop();WiFi.softAPdisconnect(true);WiFi.mode(WIFI_STA);portal=false;setupVisible=false;closingPortal=false;applyCompleteAt=0;}
    }
    if(!everConnected&&ssid.length()&&now-startedAt>45000&&!portal)openPortal();
  }
  void openPortal() {
    setupVisible=true;if(portal)return;
    WiFi.mode(WIFI_AP_STA);WiFi.softAPConfig(IPAddress(192,168,4,1),IPAddress(192,168,4,1),IPAddress(255,255,255,0));
    if(!WiFi.softAP(apName,apPassword)){Serial.println("Setup Wi-Fi failed.");setupVisible=false;return;}
    server.begin();portal=true;closingPortal=false;applyCompleteAt=0;
    Serial.printf("Setup Wi-Fi: %s | Open http://192.168.4.1 | Password is shown on screen.\n",apName);
  }
  bool command(const char *line) {
    if(!strncmp(line,"sky radius ",11)){
      double radius;Home next=home;
      if(!parseDouble(String(line+11),radius)||radius<1||radius>50||floor(radius)!=radius){Serial.println("Aircraft radius must be 1-50 whole miles.");return true;}
      next.radius=float(radius);
      if(config.putFloat("radius",next.radius)==0){Serial.println("Aircraft radius could not be saved.");return true;}
      home=next;xSemaphoreTake(mutex,portMAX_DELAY);networkHome=next;++configRevision;xSemaphoreGive(mutex);
      Serial.printf("Aircraft radius saved: %.0f miles.\n",radius);return true;
    }
    if(!strcmp(line,"wifi status")){const char *sd[]={"unavailable","wifi.txt-missing","wifi.txt-invalid","wifi.txt-loaded","filesystem-error"};Serial.printf("wifi=%s wifi_code=%d reason=%u sd=%s signal=%d dBm clock=%s\n",WiFi.status()==WL_CONNECTED?"connected":"offline",int(WiFi.status()),unsigned(disconnectReason),sd[sdState],WiFi.status()==WL_CONNECTED?int(WiFi.RSSI()):0,time(nullptr)>1700000000?"synced":"waiting");return true;}
    if(!strcmp(line,"wifi scan")){WiFi.setAutoReconnect(false);esp_wifi_disconnect();scanAt=millis()+750;Serial.println("Checking configured Wi-Fi visibility; names and credentials hidden.");return true;}
    return false;
  }
 private:
  Preferences config;WebServer server{IPAddress(192,168,4,1),80};SemaphoreHandle_t mutex=nullptr;
  String ssid,password;Home networkHome;uint32_t configRevision=0;
  uint32_t startedAt=0,applyAt=0,applyCompleteAt=0,scanAt=0;
  bool scanning=false;
  volatile uint8_t disconnectReason=0;
  bool everConnected=false,closingPortal=false;char token[24]={};
  int sdState=0;
  static void *jsonAllocate(size_t bytes){return heap_caps_malloc(bytes,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);}
  static void *tlsAllocate(size_t count,size_t size){
    if(count&&size>SIZE_MAX/count)return nullptr;
    return heap_caps_calloc(count,size,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  }
  static String html(String s){s.replace("&","&amp;");s.replace("<","&lt;");s.replace(">","&gt;");s.replace("\"","&quot;");s.replace("'","&#39;");return s;}
  void connect(){startedAt=millis();WiFi.mode(portal?WIFI_AP_STA:WIFI_STA);WiFi.setSleep(false);WiFi.STA.setScanMethod(WIFI_ALL_CHANNEL_SCAN);WiFi.setAutoReconnect(true);WiFi.begin(ssid.c_str(),password.c_str());}
  static bool parseDouble(const String &s,double &v){if(!s.length())return false;char *end;v=strtod(s.c_str(),&end);return end!=s.c_str()&&*end==0&&isfinite(v);}
  void setupRoutes() {
    server.on("/",HTTP_GET,[this](){
      String page=R"HTML(<!doctype html><html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Widget Deck setup</title><style>*{box-sizing:border-box}body{margin:0;background:#080e12;color:#e5edef;font:16px/1.5 system-ui}main{max-width:460px;margin:40px auto;padding:24px}small{color:#ffbc69;letter-spacing:.2em}h1{font-size:38px;font-weight:500;letter-spacing:-.04em}p{color:#98aeb8}label{display:block;margin:18px 0 6px}input,button{width:100%;padding:13px;border-radius:8px;border:1px solid #33434c;background:#111c23;color:inherit;font:inherit}button{margin-top:28px;background:#ffbc69;color:#14100b;border:0;font-weight:600}.pair{display:grid;grid-template-columns:1fr 1fr;gap:12px}</style><main><small>WIDGET DECK</small><h1>Connect your widgets.</h1><p>Connect the display to a 2.4 GHz Wi-Fi network. Your settings stay on this device.</p><form method="post" action="/save"><label>Wi-Fi network</label><input name="ssid" required maxlength="32" autocomplete="off" value=")HTML";
      page+=html(ssid)+"\"><label>Wi-Fi password</label><input type=\"password\" name=\"password\" maxlength=\"63\" autocomplete=\"new-password\"><p>Leave blank to keep the password for the same network.</p><div class=\"pair\"><div><label>Latitude</label><input name=\"lat\" type=\"number\" step=\"any\" min=\"-90\" max=\"90\" required value=\""+String(home.lat,7)+"\"></div><div><label>Longitude</label><input name=\"lon\" type=\"number\" step=\"any\" min=\"-180\" max=\"180\" required value=\""+String(home.lon,7)+"\"></div></div><label>Search radius (miles)</label><input type=\"number\" name=\"radius\" min=\"1\" max=\"50\" step=\"1\" required value=\""+String(home.radius,0)+"\"><input type=\"hidden\" name=\"token\" value=\""+token+"\"><button>Save and connect</button></form><p>Saved setup survives restart. Editing wifi.txt on the SD card replaces these settings at the next restart.</p><p>Aircraft data: ADSB.lol (ODbL). Only your search coordinates are sent to the aircraft service.</p></main></html>";
      server.sendHeader("Cache-Control","no-store");server.send(200,"text/html",page);
    });
    server.on("/save",HTTP_POST,[this](){
      if(server.arg("token")!=token){server.send(403,"text/plain","Please reopen the setup page.");return;}
      double lat,lon,radius;String nextSsid=server.arg("ssid"),nextPass=server.arg("password");
      if(!parseDouble(server.arg("lat"),lat)||!parseDouble(server.arg("lon"),lon)||!parseDouble(server.arg("radius"),radius)||!validHome(Home{lat,lon,float(radius)})||!nextSsid.length()||nextSsid.length()>32||nextPass.length()>63||(nextPass.length()>0&&nextPass.length()<8)) {server.send(400,"text/plain","Check coordinates, radius (1-50 miles), network name and password (8-63 characters or blank).");return;}
      if(nextSsid==ssid&&!nextPass.length())nextPass=password;
      ssid=nextSsid;password=nextPass;home={lat,lon,float(radius)};
      config.putString("ssid",ssid);config.putString("pass",password);config.putDouble("lat",lat);config.putDouble("lon",lon);config.putFloat("radius",home.radius);
      xSemaphoreTake(mutex,portMAX_DELAY);networkHome=home;++configRevision;xSemaphoreGive(mutex);
      Serial.println("Home Wi-Fi settings saved (credentials hidden). Connecting.");
      WiFi.setAutoReconnect(false);esp_wifi_disconnect();
      closingPortal=true;applyCompleteAt=0;applyAt=millis()+750;
      server.sendHeader("Cache-Control","no-store");server.send(200,"text/html","<meta name='viewport' content='width=device-width,initial-scale=1'><body style='background:#080e12;color:#e5edef;font:20px system-ui;padding:40px'><h1>Settings saved.</h1><p>The screen will connect and start looking for aircraft. The setup network closes after a successful Wi-Fi connection.</p><p>If it stays on the setup screen, return to <a href='/' style='color:#ffbc69'>settings</a> and check the network password.</p></body>");
    });
    server.onNotFound([this](){server.send(404,"text/plain","Open http://192.168.4.1 to set up Widget Deck.");});
  }
};
}
