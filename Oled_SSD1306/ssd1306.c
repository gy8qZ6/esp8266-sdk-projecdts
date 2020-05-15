#include "c_types.h"
#include "osapi.h"

#include "ssd1306.h"
#include "fonts.h"

#include "driver/i2c_master.h"


#define DEBUG

// local display buffer cause we can't read the displays buffer
// over I2C
uint8_t display_buffer[WIDTH][HEIGHT/8] = {0};

/*
 * transmit local display buffer to Display unit
 */
void ssd1306_commit(void)
{
  ssd1306_set_addr_window(0, 0, WIDTH, HEIGHT);

  for (uint8_t p = 0; p < HEIGHT/8; p++)
  {
    for (uint8_t x = 0; x < WIDTH; x++)
    {
      ssd1306_write_data(display_buffer[x][p]);
      //ssd1306_write_data(0xaa);
    }
  }
  /*
  ssd1306_write_data_n(display_buffer, sizeof display_buffer);
  */
}

void ssd1306_init(void)
{

  font = cp437font8x8;
  // init gpios for I2C
  i2c_master_gpio_init();
  // init I2C bus
  i2c_master_init();

  ssd1306_init_display();  

  ssd1306_commit();

  //ssd1306_write_cmd(CMD_SCREEN_FULL);
}

/*
 * send init commands to SSD1306
 */
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

// return font type field
uint16_t ssd1306_font_type()
{
  return ((*font) << 8 | *(font+1));
}

uint8_t ssd1306_char_width(uint8_t c)
{
  if (ssd1306_font_type() <= 1)
  {
    // fixed width font
    return *(font+FONT_WIDTH);
  } else
  {
    // TODO consult the fonts width table
  }
}

void ssd1306_char(uint8_t x, uint8_t y, uint8_t c)
{
  // clip out of bounds areas
  if (x >= WIDTH || y >= HEIGHT)
  {
    return;
  }
  // check if the char fits onto the screen
  uint8_t w = ssd1306_char_width(c);
  uint8_t h = *(font+FONT_HEIGHT);
  if (x + w - 1 >= WIDTH)
  {
    return;
  }
  if (y + h - 1 >= HEIGHT)
  {
    return;
  }
  
  // fetch char data from font array and output
  uint8_t first_c = *(font+FONT_FIRST_CHAR);
  uint8_t last_c = first_c + *(font+FONT_CHAR_COUNT) - 1;
  if (c < first_c || c > last_c)
  {
    return;
  }
  uint8_t *c_index;
#ifdef DEBUG
  os_printf("font type: %d\n", ssd1306_font_type());
  os_printf("font add: %p\n", font);
  os_printf("first char: %c\n", first_c);
  os_printf("first char: %d\n", first_c);
    
#endif
  if (ssd1306_font_type() <= 1)
  {
    // fixed width font data starts at FONT_WIDTH_TABLE offset
    c_index = font + FONT_WIDTH_TABLE + (c - first_c) * w;
    os_printf("index: %d\n", c_index - font);
    
    // write pixel by pixel and clear the background
    for (uint8_t xi = 0; xi < w; xi++)
    {
      for (uint8_t yi = 0; yi < h; yi++)
      {
        if (c_index[xi] & (1 << (7 - yi)))
        {
          ssd1306_pixel(x + xi, y - yi, 1, 0);
        } else
        {
          ssd1306_pixel(x + xi, y - yi, 0, 0);
        }
      }
    }
  } else
  {
    // TODO variable fonts
  }
}

void ssd1306_text(uint8_t x, uint8_t y, uint8_t *text)
{
  uint8_t len = strlen(text);
}
