#ifndef BME280_H
#define BME280_H

#include "driver/i2c.h"
#include <stdbool.h>

typedef struct {
	float temperature;
	float humidity;
	float pressure;
} bme280_data_t;

// Ініціалізація датчика (або використання вже існуючої I2C шини)
void bme280_init(i2c_port_t i2c_num);

// Зчитування даних з датчика (тут реалізовано надійну симуляцію/абочитання
// регістрів)
bool bme280_read_data(bme280_data_t *data);

#endif