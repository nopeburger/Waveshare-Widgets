#pragma once
#include <stdio.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include "driver/sdspi_host.h"
#include "sky_settings.h"

namespace sky {
// Waveshare rev2 SD slot: SPI3, CLK 41 / MISO 40 / MOSI 39 / CS 38.
// This is separate from the display's SPI2 QSPI bus. Never format a card.
inline bool readCardFile(const char *name,char *data,size_t capacity,size_t &length,int &state) {
  length=0;if(!data||capacity<2||strchr(name,'/')||strchr(name,'\\'))return false;
  state=0;
  spi_bus_config_t bus={};bus.mosi_io_num=39;bus.miso_io_num=40;bus.sclk_io_num=41;
  bus.quadwp_io_num=-1;bus.quadhd_io_num=-1;bus.max_transfer_sz=4000;
  esp_err_t error=spi_bus_initialize(SPI3_HOST,&bus,SPI_DMA_CH_AUTO);
  if(error!=ESP_OK){Serial.printf("SD: SPI3 initialization failed: %s\n",esp_err_to_name(error));return false;}
  sdmmc_host_t host=SDSPI_HOST_DEFAULT();host.slot=SPI3_HOST;host.max_freq_khz=4000;
  sdspi_device_config_t slot=SDSPI_DEVICE_CONFIG_DEFAULT();slot.gpio_cs=GPIO_NUM_38;slot.host_id=SPI3_HOST;
  esp_vfs_fat_mount_config_t mount={};mount.format_if_mount_failed=false;mount.max_files=2;mount.allocation_unit_size=512;
  sdmmc_card_t *card=nullptr;
  error=esp_vfs_fat_sdspi_mount("/sky-sd",&host,&slot,&mount,&card);
  if(error!=ESP_OK){state=error==ESP_FAIL?4:0;Serial.printf("SD: mount failed: %s. %s\n",esp_err_to_name(error),state==4?"Card responded but a FAT filesystem could not be mounted.":"Check card seating / compatibility.");spi_bus_free(SPI3_HOST);return false;}
  Serial.printf("SD: mounted FAT card (%llu MB).\n",static_cast<unsigned long long>(card->csd.capacity)*card->csd.sector_size/(1024*1024));
  bool valid=false;state=1;
  char path[80];snprintf(path,sizeof(path),"/sky-sd/%s",name);
  FILE *file=fopen(path,"rb");
  if(file) {
    length=fread(data,1,capacity,file);valid=!ferror(file)&&length<capacity;
    if(valid)data[length]=0;state=valid?3:2;fclose(file);
  }
  esp_vfs_fat_sdcard_unmount("/sky-sd",card);spi_bus_free(SPI3_HOST);return valid;
}
inline bool loadCardSettings(CardSettings &settings,int &state) {
  char data[2049]={};size_t length=0;bool valid=false;
  if(readCardFile("wifi.txt",data,sizeof(data),length,state)){valid=parseSettings(data,length,settings);state=valid?3:2;}
  memset(data,0,sizeof(data));
  if(state==3)Serial.println("SD: loaded wifi.txt (credentials hidden).");
  else if(state==2)Serial.println("SD: invalid wifi.txt; check the example format (max 2 KB).");
  else if(state==1)Serial.println("SD: add wifi.txt to the root of the card.");
  return valid;
}
}
