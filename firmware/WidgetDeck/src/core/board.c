#include "board.h"
#include <assert.h>
#include "esp_lcd_panel_io.h"
#include "esp_lcd_sh8601.h"
#include "esp_lcd_panel_ops.h"
#include "driver/spi_master.h"
#include "driver/i2c.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_check.h"

// Pins, CO5300 init sequence and x offset from Waveshare rev2 example.
static esp_lcd_panel_handle_t panel;
static esp_lcd_panel_io_handle_t io;
#define STRIPE_ROWS 32
static SemaphoreHandle_t complete;
static uint16_t *stripe[2];
static const sh8601_lcd_init_cmd_t init[] = {
  {0x11,(uint8_t[]){0},0,80}, {0xC4,(uint8_t[]){0x80},1,0},
  {0x35,(uint8_t[]){0},1,0}, {0x53,(uint8_t[]){0x20},1,1},
  {0x63,(uint8_t[]){0xff},1,1}, {0x51,(uint8_t[]){0},1,1},
  {0x29,(uint8_t[]){0},0,10}
};
static bool transferred(esp_lcd_panel_io_handle_t handle,esp_lcd_panel_io_event_data_t *event,void *context) {
  BaseType_t wake=pdFALSE;
  xSemaphoreGiveFromISR(complete,&wake);
  return wake==pdTRUE;
}
void board_init(void) {
  complete=xSemaphoreCreateCounting(2,0);assert(complete);
  stripe[0]=heap_caps_malloc(280*STRIPE_ROWS*2,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
  stripe[1]=heap_caps_malloc(280*STRIPE_ROWS*2,MALLOC_CAP_DMA|MALLOC_CAP_INTERNAL);
  assert(stripe[0]&&stripe[1]);
  spi_bus_config_t bus=SH8601_PANEL_BUS_QSPI_CONFIG(10,11,12,13,14,280*STRIPE_ROWS*2);
  ESP_ERROR_CHECK(spi_bus_initialize(SPI2_HOST,&bus,SPI_DMA_CH_AUTO));
  esp_lcd_panel_io_spi_config_t conf=SH8601_PANEL_IO_QSPI_CONFIG(46,transferred,NULL);
  ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI2_HOST,&conf,&io));
  sh8601_vendor_config_t vendor={.init_cmds=init,.init_cmds_size=sizeof(init)/sizeof(init[0]),.flags.use_qspi_interface=1};
  esp_lcd_panel_dev_config_t dev={.reset_gpio_num=21,.rgb_ele_order=LCD_RGB_ELEMENT_ORDER_RGB,.bits_per_pixel=16,.vendor_config=&vendor};
  ESP_ERROR_CHECK(esp_lcd_new_panel_sh8601(io,&dev,&panel));
  ESP_ERROR_CHECK(esp_lcd_panel_reset(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_init(panel));
  ESP_ERROR_CHECK(esp_lcd_panel_set_gap(panel,20,0));
  ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel,true));
  i2c_config_t touch={.mode=I2C_MODE_MASTER,.sda_io_num=47,.scl_io_num=48,
    .sda_pullup_en=GPIO_PULLUP_ENABLE,.scl_pullup_en=GPIO_PULLUP_ENABLE,.master.clk_speed=300000};
  ESP_ERROR_CHECK(i2c_param_config(I2C_NUM_0,&touch));
  ESP_ERROR_CHECK(i2c_driver_install(I2C_NUM_0,touch.mode,0,0,0));
  uint8_t normal[]={0,0};
  ESP_ERROR_CHECK(i2c_master_write_to_device(I2C_NUM_0,0x38,normal,2,pdMS_TO_TICKS(50)));
}
static void present(const uint16_t *pixels,int scale_shift) {
  int queued=0;
  for(int y=0;y<456;y+=STRIPE_ROWS) {
    int rows=(456-y<STRIPE_ROWS)?456-y:STRIPE_ROWS;
    int slot=queued&1;
    if(queued>=2 && xSemaphoreTake(complete,pdMS_TO_TICKS(1000))!=pdTRUE) ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
    // Panel expects high byte first. Render buffer remains native RGB565.
    for(int py=0;py<rows;py++)for(int x=0;x<280;x++) {
      uint16_t p=pixels[((y+py)>>scale_shift)*(280>>scale_shift)+(x>>scale_shift)];
      stripe[slot][py*280+x]=(p>>8)|(p<<8);
    }
    ESP_ERROR_CHECK(esp_lcd_panel_draw_bitmap(panel,0,y,280,y+rows,stripe[slot]));
    queued++;
  }
  int pending=queued<2?queued:2;
  while(pending-- && xSemaphoreTake(complete,pdMS_TO_TICKS(1000))!=pdTRUE) ESP_ERROR_CHECK(ESP_ERR_TIMEOUT);
}
void board_present(const uint16_t *pixels) {present(pixels,1);}
void board_present_native(const uint16_t *pixels) {present(pixels,0);}
void board_brightness(uint8_t value) {ESP_ERROR_CHECK(esp_lcd_panel_io_tx_param(io,0x02005100,&value,1));}
int board_touch(int *x,int *y) {
  uint8_t reg=2,data[5]={0};
  esp_err_t result=i2c_master_write_read_device(I2C_NUM_0,0x38,&reg,1,data,5,pdMS_TO_TICKS(12));
  if(result!=ESP_OK)return -1;
  if(!(data[0]&15))return 0;
  int xx=((data[1]&15)<<8)|data[2],yy=((data[3]&15)<<8)|data[4];
  if(xx>=280||yy>=456)return -1;
  *x=xx;*y=yy;return 1;
}
