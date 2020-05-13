#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"

#include "driver/uart.h"

//#define blink
#define serial

#if defined blink && defined serial
  #error "can't do blink and serial comm simultaneously"
#endif

extern int ets_uart_printf(const char *fmt, ...);

// ESP-12 modules have LED on GPIO2. Change to another GPIO
// for other boards.
static const int pin = 2;
static volatile os_timer_t some_timer;

void ICACHE_FLASH_ATTR some_timerfunc(void *arg)
{
  
#ifdef serial
  os_printf("os_printf(): hello world\n");
  ets_uart_printf("ets_uart_printf():Hello World!\r\n");
#endif

#ifdef blink
  //Do blinky stuff
  if (GPIO_REG_READ(GPIO_OUT_ADDRESS) & (1 << pin))
  {
    // set gpio low
    gpio_output_set(0, (1 << pin), 0, 0);
  }
  else
  {
    // set gpio high
    gpio_output_set((1 << pin), 0, 0, 0);
  }
#endif
}

void ICACHE_FLASH_ATTR user_init()
{
#ifdef blink
  // init gpio subsytem
  gpio_init();

  // configure UART TXD to be GPIO1, set as output
  PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_GPIO1); 
  gpio_output_set(0, 0, (1 << pin), 0);
#endif

#ifdef serial
  // Initialize UART0
  //works also: 
  uart_div_modify(0, UART_CLK_FREQ / 115200);
  //uart_init(BIT_RATE_115200, BIT_RATE_115200);
  //uart_init(BIT_RATE_74880, BIT_RATE_74880);
  os_printf("os_printf(): hello world\n");
  ets_uart_printf("ets_uart_printf():Hello World!\r\n");
#endif

  // setup timer (500ms, repeating)
  os_timer_setfn(&some_timer, (os_timer_func_t *)some_timerfunc, NULL);
  os_timer_arm(&some_timer, 1000, 1);
}
