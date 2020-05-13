#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"

#include "driver/uart.h"
#include "user_interface.h"

#define serial

#define WIFI_MODE_STATION 0x01
#define WIFI_MODE_SOFTAP 0x02
#define WIFI_MODE_STATION_AND_SOFTAP 0x03

// ESP-12 modules have LED on GPIO2. Change to another GPIO
// for other boards.
static const int pin = 2;
static volatile os_timer_t some_timer;

void scan_done(void *arg, STATUS status)
{
  os_printf("scan done\r\n");
}

void ICACHE_FLASH_ATTR some_timerfunc(void *arg)
{
  wifi_scan_type_t scan_type = WIFI_SCAN_TYPE_PASSIVE;
  struct scan_config scan_conf = {
    .ssid = NULL,
    .bssid = NULL,
    .channel = 0,
    .show_hidden = 1,
    .scan_type = scan_type,
  };
  wifi_station_scan (&scan_conf, scan_done);
}

void ICACHE_FLASH_ATTR user_init()
{
#ifdef serial
  // Initialize UART0
  //works also: 
  uart_div_modify(0, UART_CLK_FREQ / 115200);
  //uart_init(BIT_RATE_115200, BIT_RATE_115200);
  //uart_init(BIT_RATE_74880, BIT_RATE_74880);
  os_printf("os_printf(): hello world\n");
  ets_uart_printf("ets_uart_printf():Hello World!\r\n");
#endif

  wifi_set_opmode(WIFI_MODE_STATION);

  // setup timer (500ms, repeating)
  os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);
  os_timer_arm(&some_timer, 10000, 1);
}
