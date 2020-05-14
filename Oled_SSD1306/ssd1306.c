#include "c_types.h"

#include "ssd1306.h"

#include "driver/i2c_master.h"
/*
b7 b6 b5 b4 b3 b2 b1 b0
0 1 1 1 1 0 SA0 R/W#
SA0: 0 or 1
R/W: 1/0
*/

void ssd1306_init(void)
{
  // init gpios for I2C
  i2c_master_gpio_init();
  // init I2C bus
  i2c_master_init();

  ssd1306_init_display();  
}

/*
void ssd1306_test(void)
{
  const uint8_t dlist1[] = {
    SSD1306_PAGEADDR,
    0,                         // Page start address
    0xFF,                      // Page end (not really, but works here)
    SSD1306_COLUMNADDR,
    0 };                       // Column start address

  for (uint8_t i = 0; i < sizeof dlist1; i++)
  {
    ssd1306_write_cmd(dlist1[i]);
  }
  ssd1306_write_cmd(WIDTH - 1); // Column end address
  uint8_t *ptr   = buffer;
    wire->beginTransmission(i2caddr);
    WIRE_WRITE((uint8_t)0x40);
    WIRE_WRITE(0xFF);
    wire->endTransmission();

}
*/

void ssd1306_init_display(void)
{
  for (uint8_t i = 0; i < sizeof init_data_128x32; i++)
  {
    ssd1306_write_cmd(init_data_128x32[i]);
  }
}
/*
void ssd1306_write_data(uint8_t c)
{
  i2c_master_start();

  // start command
  i2c_master_writeByte(MODE_WRITE);

  // rcv ACK
  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  // write cmd
  i2c_master_writeByte(c);

  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  i2c_master_stop();
}
*/
void ssd1306_write_cmd(uint8_t c)
{
  i2c_master_start();

  // start command
  i2c_master_writeByte(MODE_WRITE);

  // rcv ACK
  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  // tx control byte
  i2c_master_writeByte(CTRL_BYTE_CMD);

  // rcv ACK
  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  // write cmd
  i2c_master_writeByte(c);

  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  i2c_master_stop();
}

void ssd1306_turn_on(void)
{
  // DC: 0 0xA4: resume Display on
  ssd1306_write_cmd(0xA4);
}
