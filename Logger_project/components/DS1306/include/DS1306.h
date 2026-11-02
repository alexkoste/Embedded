#ifndef DS1306_H
#define DS1306_H

#include "driver/i2c.h"
#include <stdint.h>

void ds1306_init(void);
esp_err_t ds1306_get_timestamp(uint32_t *timestamp);

#endif