

#include "ssd1306.h"
#include "c_types.h"

#include "driver/i2c_master.h"
/*
b7 b6 b5 b4 b3 b2 b1 b0
0 1 1 1 1 0 SA0 R/W#
SA0: 0 or 1
R/W: 1/0
*/

#define SLAVE_ADDR 0x3C   //011 1100
#define WRITE_START (SLAVE_ADDR << 1)

void ssd1306_turn_on(void)
{
  // init gpios for I2C
  i2c_master_gpio_init();

  // init I2C bus
  i2c_master_init();

  i2c_master_start();

  // start command
  i2c_master_writeByte(WRITE_START);

  // rcv ACK
  if (!(i2c_master_checkAck()))
  {
    perror("NACK\n");
  }


  // Co:1 DC:0
  i2c_master_writeByte(0x2 << 6);
  if (!(i2c_master_checkAck()))
  {
    perror("NACK\n");
  }
  // DC: 0 0xA4: resume Display on
  i2c_master_writeByte(0xA4);
  if (!(i2c_master_checkAck()))
  {
    perror("NACK\n");
  }

  i2c_master_stop();
}
