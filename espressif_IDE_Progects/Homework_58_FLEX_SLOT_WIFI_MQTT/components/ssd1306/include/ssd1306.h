#ifndef SSD1306_H
#define SSD1306_H

#include "driver/i2c.h"

#define SSD1306_I2C_ADDR 0x3C // Якщо не працює, спробуйте 0x3D
#define SSD1306_WIDTH 128
#define SSD1306_HEIGHT 64

void ssd1306_init(i2c_port_t i2c_num, int sda_io, int scl_io);
void ssd1306_clear(void);
void ssd1306_update_telemetry(const char *prefix, const char *student,
							  float temp, float pres, float hum);

#endif