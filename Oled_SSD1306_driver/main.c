#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"
#include "driver/uart.h"

#include "ssd1306.h"

static volatile os_timer_t some_timer;
static uint8_t x = 0, y = 0;
static uint8_t state = 1;

void ICACHE_FLASH_ATTR some_timerfunc(void *arg)
{
  
  /*
  ssd1306_pixel(x, y, state, 1);
  //ssd1306_commit();

  y++;
  if (y == 32)
  {
    x++;
    y = 0;
  }
  if (x == 128)
  {
    x = 0;
    state = !state;
  }
  */
}

void ICACHE_FLASH_ATTR user_init()
{

  // enable serial debugging
  //uart_div_modify(0, UART_CLK_FREQ / 115200);
  uart_init(BIT_RATE_115200, BIT_RATE_115200);
  
  ssd1306_init();

  uint8_t str[100] = {'H','i',' ','J','e','n','n','y',' ',3,3,3,'\0'};
  
  for (uint8_t k = 0; k < 100; k++)
  {
    str[k] = k+1;
  }
  str[60] = '\0';
  /*

  for (uint8_t i = 0; i < sizeof str; i++)
    {
      ssd1306_char(30 + i*8, 15, str[i]);
    }
  */
  ssd1306_text(0,7,str, 0);
  //ssd1306_pixel(0,0,1,0);
  ssd1306_commit();
  
  /*
//  os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);
//  os_timer_arm(&some_timer, 3, 1);
  while (1)
  {
  for (uint8_t x = 0; x < 128; x++)
  {
    for (uint8_t y = 0; y < 32; y++)
      ssd1306_pixel(x,y,state,1);
    system_soft_wdt_feed();
  }
  state = !state;
  }
  */
}



