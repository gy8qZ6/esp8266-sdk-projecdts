#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"

#include "ssd1306.h"

void ICACHE_FLASH_ATTR user_init()
{
  ssd1306_turn_on();
}
