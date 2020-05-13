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

void ICACHE_FLASH_ATTR
user_rf_pre_init(void)
{
}
/******************************************************************************
 * FunctionName : user_rf_cal_sector_set
 * Description  : SDK just reversed 4 sectors, used for rf init data and paramters.
 *                We add this function to force users to set rf cal sector, since
 *                we don't know which sector is free in user's application.
 *                sector map for last several sectors : ABCCC
 *                A : rf cal
 *                B : rf init data
 *                C : sdk parameters
 * Parameters   : none
 * Returns      : rf cal sector
*******************************************************************************/
uint32 ICACHE_FLASH_ATTR
user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map) {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_64M_MAP_1024_1024:
            rf_cal_sec = 2048 - 5;
            break;
        case FLASH_SIZE_128M_MAP_1024_1024:
            rf_cal_sec = 4096 - 5;
            break;
        default:
            rf_cal_sec = 0;
            break;
    }

    return rf_cal_sec;
}

void ICACHE_FLASH_ATTR scan_done(void *arg, STATUS status)
{
  //os_printf("scan done\r\n");
  struct bss_info *res = (struct bss_info*) arg;

  uint16_t number_aps = 0;
  uint8_t channels[15] = {0}; // channels 1 - 14 on indizes 1 - 14
  while (res != NULL)
  {
    // get the APs SSID or ESSID if empty name
    if (res->ssid_len)
    {
      os_printf("\'%s\'", res->ssid);
      if (strlen(res->ssid) == 0)
      {
        // SSID is '' but res->ssid_len reports 21?!
        // investigate:
        // SSID is indeed 21 null bytes 
        os_printf("##### ");
        uint8_t *p = res->ssid;
        for (uint8_t i = 0; i < res->ssid_len; i++)
        {
          os_printf("0x%02x ", *p);
          p++;
        }
        os_printf(MACSTR, MAC2STR(res->bssid));
        os_printf("auth: %d\n", res->authmode);
      }
    }else
    {
      os_printf(MACSTR, MAC2STR(res->bssid));
    }
    os_printf(" strlen: %d\n", res->ssid_len);
    channels[res->channel]++;
    number_aps++;
    //res = &(res->next);
    res = STAILQ_NEXT(res, next);
  }
  os_printf("found %d APs\n", number_aps);
  for (uint8_t i = 1; i < sizeof channels; i++)
  {
    os_printf("%d ", channels[i]);
  }
  os_printf("\n");
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
  os_printf("Scanning...\n");
  wifi_station_scan (&scan_conf, scan_done);
}

void ICACHE_FLASH_ATTR user_init()
{
  system_phy_set_powerup_option(3);
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
