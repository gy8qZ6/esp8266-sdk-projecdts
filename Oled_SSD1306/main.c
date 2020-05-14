#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"

#include "ssd1306.h"

static volatile os_timer_t some_timer;
static uint8_t x = 0, y = 0;
static uint8_t state = 1;

void ICACHE_FLASH_ATTR some_timerfunc(void *arg)
{
  
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
}

void ICACHE_FLASH_ATTR user_init()
{
  ssd1306_init();

  os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);
  os_timer_arm(&some_timer, 3, 1);
}
