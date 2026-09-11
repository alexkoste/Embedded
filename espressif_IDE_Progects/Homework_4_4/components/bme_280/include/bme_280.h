#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

// Нова конфігурація пінів для ESP32-S3 за вашим запитом
#define PIN_NUM_MISO 5 // SDO
#define PIN_NUM_CS 6   // CSB
#define PIN_NUM_MOSI 7 // SDA (SDI)
#define PIN_NUM_CLK 15 // SCL (SCK)

// Регістри BME280
#define BME280_REG_CHIPID 0xD0
#define BME280_REG_CTRL_HUM 0xF2
#define BME280_REG_CTRL_MEAS 0xF4
#define BME280_REG_CONFIG 0xF5
#define BME280_REG_DATA 0xF7

typedef struct {
	uint16_t dig_T1;
	int16_t dig_T2;
	int16_t dig_T3;
	uint16_t dig_P1;
	int16_t dig_P2;
	int16_t dig_P3;
	int16_t dig_P4;
	int16_t dig_P5;
	int16_t dig_P6;
	int16_t dig_P7;
	int16_t dig_P8;
	int16_t dig_P9;
	uint8_t dig_H1;
	int16_t dig_H2;
	uint8_t dig_H3;
	int16_t dig_H4;
	int16_t dig_H5;
	int8_t dig_H6;
	int32_t t_fine;
} bme280_calib_t;

typedef struct {
	float temperature;
	float pressure;
	float humidity;
} bme280_data;

bme280_data bme_280_read_data();
void spi_and_devcie_init();

#ifdef __cplusplus
}
#endif