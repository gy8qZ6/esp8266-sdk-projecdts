#include "c_types.h"
#include "osapi.h"

#include "ssd1306.h"

#include "driver/i2c_master.h"

// local display buffer cause we can't read the displays buffer
// over I2C
uint8_t display_buffer[WIDTH][HEIGHT/8] = {0};

/*
 * transmit local display buffer to Display unit
 */
void ssd1306_commit(void)
{
  ssd1306_set_addr_window(0, 0, WIDTH, HEIGHT);

  /*
  for (uint8_t p = 0; p < HEIGHT/8; p++)
  {
    for (uint8_t x = 0; x < WIDTH; x++)
    {
      ssd1306_write_data(display_buffer[x][p]);
      //ssd1306_write_data(0xaa);
    }
  }
  */
  ssd1306_write_data_n(display_buffer, sizeof display_buffer);
}

void ssd1306_init(void)
{
  // init gpios for I2C
  i2c_master_gpio_init();
  // init I2C bus
  i2c_master_init();

  ssd1306_init_display();  
  //i2c_master_wait(99000);
  ssd1306_commit();

  //ssd1306_write_cmd(CMD_SCREEN_FULL);
}

void ssd1306_init_display(void)
{
  for (uint8_t i = 0; i < sizeof init_data_128x32; i++)
  {
    ssd1306_write_cmd(init_data_128x32[i]);
  }
}

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

  // tx data byte
  i2c_master_writeByte(CTRL_BYTE_DATA);
  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  // write data
  i2c_master_writeByte(c);

  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  i2c_master_stop();
}

void ssd1306_write_data_n(uint8_t *c_list, uint16_t len)
{
  i2c_master_start();

  // start command
  i2c_master_writeByte(MODE_WRITE);

  // rcv ACK
  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  // tx data byte
  i2c_master_writeByte(CTRL_BYTE_DATA);
  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  for (; len > 0; len--, c_list++)
  {

    // write data
    i2c_master_writeByte(*c_list);

    // rcv ACK
    if (!(i2c_master_checkAck()))
    {
      //perror("NACK\n");
    }
  }

  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  i2c_master_stop();
}

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

  // rcv ACK
  if (!(i2c_master_checkAck()))
  {
    //perror("NACK\n");
  }

  i2c_master_stop();
}

void ssd1306_write_cmd_n(uint8_t *c_list, uint16_t len)
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
  for (; len > 0; len--, c_list++)
  {

    // write cmd
    i2c_master_writeByte(*c_list);

    // rcv ACK
    if (!(i2c_master_checkAck()))
    {
      //perror("NACK\n");
    }
  }

  i2c_master_stop();
}

void ssd1306_pixel(uint8_t x, uint8_t y, uint8_t state, uint8_t immediate)
{
  // manipulate local buffer
  if (state)
  {
    // turn on pixel
    display_buffer[x][y/8] |= 1 << (y % 8);
  } else
  {
    // turn off pixel
    display_buffer[x][y/8] &= ~(1 << (y % 8));
  }

  // update display now
  if (immediate)
  {
    // set up display area to update
    ssd1306_set_addr_window(x, y, 1, 1);

    // send command to enable/disable pixel
    ssd1306_write_data(display_buffer[x][y/8]);
  }
}
/*
 * set up the area we want to update
 */
void ssd1306_set_addr_window(uint8_t x, uint8_t y, uint8_t width, uint8_t height)
{
  // clip out of bounds areas
  if (x >= WIDTH || y >= HEIGHT || width == 0 || height == 0)
  {
    return;
  }
  if (x + width - 1 >= WIDTH)
  {
    width = WIDTH - x;
  }
  if (y + height - 1 >= HEIGHT)
  {
    height = HEIGHT - y;
  }

  uint8_t cmds[] = { 
    SET_COL_ADDR, x, x + width - 1,
    SET_PAGE_ADDR, y / 8, (y + height - 1) / 8,
  };
  ssd1306_write_cmd_n(cmds, sizeof cmds);
}
