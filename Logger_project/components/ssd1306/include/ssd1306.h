#ifndef SSD1306_H
#define SSD1306_H

#include "driver/i2c.h"

void ssd1306_init(i2c_port_t i2c_num);
void ssd1306_clear(void);
void ssd1306_draw_string(uint8_t x, uint8_t line, const char *str);
void ssd1306_refresh(void);

#endif