
#ifndef SSD1306_H
#define SSD1306_H

#define HEIGHT              32
#define WIDTH               128

#define SLAVE_ADDR          0x3C
#define MODE_WRITE          (SLAVE_ADDR << 1)
#define CTRL_BYTE_DATA      0x40
#define CTRL_BYTE_CMD       0x00

#define CMD_MUX_RATIO       0xA8
#define CMD_COM_PIN_CONF    0xDA
#define CMD_CONTRAST        0x81
#define CMD_SCREEN_RAM      0xA4
#define CMD_SCREEN_FULL     0xA5
#define CMD_INVERT_OFF      0xA6
#define CMD_INVERT_ON       0xA7
#define CMD_DCLK_DR         0xD5 
#define CMD_CHARGE_PUMP     0x8D
#define CMD_DSP_ON          0xAF
#define CMD_DSP_OFF         0xAE
#define CMD_DSP_OFFSET      0xD3
#define CMD_DSP_STARTLN     0x40
#define CMD_SEG_REMAP_0     0xA0
#define CMD_SEG_REMAP_127   0xA1
#define CMD_COM_OP_DIR_FW   0xC0
#define CMD_COM_OP_DIR_BW   0xC8
#define CMD_CONTRAST        0x81

#define CMD_MEM_ADDR_MODE   0x20
#define CMD_PRE_CHARGE      0xD9
#define CMD_VCOM_DETECT     0xDB

#define SET_LOW_COL_ADDR    0x00
#define SET_HIGH_COL_ADDR   0x10
#define SET_COL_ADDR        0x21
#define SET_PAGE_ADDR       0x22

static const uint8_t init_data_128x32[] = { CMD_DSP_OFF,
    CMD_MUX_RATIO, HEIGHT-1,    // set MUX ratio: A8, 3F
    //CMD_DSP_OFFSET, 0x00,       // reset to 0x00    // set display offset: D3, 00
    //CMD_DSP_STARTLN,            // reset to 0x00    // set display start line: 40
    //CMD_SEG_REMAP_0,            // reset to col0 == SEG0 // set segment remap: A0/A1
    //CMD_COM_OP_DIR_FW,          // reset to normal mode // set com output scan direction: C0/C8
    CMD_COM_PIN_CONF, 0x02,     // set com pins hardware conf: DA, 02
    CMD_MEM_ADDR_MODE, 0x00,
    //CMD_CONTRAST, 0x7F,         // reset to 7F      // set contrast control: 81, 7F CMD_SCREEN_RAM,     // disable entire display on: A4
    CMD_INVERT_OFF,     // set normal display: A6
    //CMD_DCLK_DR, 0x80,// reset is 80      // set osc freq: D5, 80
    CMD_CHARGE_PUMP, 0x14,  // enable charge pump regulator: 8D, 14
    CMD_DSP_ON,// display on: AF
};

static const uint8_t *font;

void ssd1306_init(void);
void ssd1306_init_display(void);  
void ssd1306_write_cmd(uint8_t c);
void ssd1306_write_cmd_n(uint8_t *c_list, uint16_t len);
void ssd1306_write_data(uint8_t c);
void ssd1306_write_data_n(uint8_t *c_list, uint16_t len);
void ssd1306_pixel(uint8_t x, uint8_t y, uint8_t state, uint8_t immediate);
void ssd1306_set_addr_window(uint8_t x, uint8_t y, uint8_t width, uint8_t height);
void ssd1306_commit(void);

uint16_t ssd1306_font_type(void);
uint8_t ssd1306_char_width(uint8_t c);
void ssd1306_char(uint8_t x, uint8_t y, uint8_t c, uint8_t invert);
void ssd1306_text(uint8_t x, uint8_t y, uint8_t *text, uint8_t invert);

#endif /* SSD1306_H */
