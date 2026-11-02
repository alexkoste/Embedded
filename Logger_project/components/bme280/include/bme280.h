#ifndef BME280_H
#define BME280_H

#include "driver/i2c.h"

esp_err_t bme280_init(i2c_port_t i2c_num, uint8_t addr);
esp_err_t bme280_read_data(i2c_port_t i2c_num, float *temp, float *hum,
						   float *press);

#endif