#include "include/bme_280.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

bme280_calib_t calib;
spi_device_handle_t spi;

void bme280_write_reg(uint8_t reg, uint8_t value) {
	spi_transaction_t t = {.flags = SPI_TRANS_USE_TXDATA,
						   .length = 16,
						   .tx_data = {reg & 0x7F, value}};
	spi_device_polling_transmit(spi, &t);
}

// Читання масиву байтів з регістру (для читання bit 7 = 1)
void bme280_read_regs(uint8_t reg, uint8_t *data, size_t len) {
	uint8_t tx_buf[len + 1];
	uint8_t rx_buf[len + 1];

	tx_buf[0] = reg | 0x80; // Встановлюємо біт читання
	for (size_t i = 1; i <= len; i++)
		tx_buf[i] = 0;

	spi_transaction_t t = {
		.length = (len + 1) * 8,
		.tx_buffer = tx_buf,
		.rx_buffer = rx_buf,
	};
	spi_device_polling_transmit(spi, &t);

	for (size_t i = 0; i < len; i++) {
		data[i] = rx_buf[i + 1];
	}
}

// Зчитування калібрувальних значень з пам'яті BME280
void bme280_read_calibration(void) {
	uint8_t buf[24];
	bme280_read_regs(0x88, buf, 24);

	calib.dig_T1 = (buf[1] << 8) | buf[0];
	calib.dig_T2 = (buf[3] << 8) | buf[2];
	calib.dig_T3 = (buf[5] << 8) | buf[4];
	calib.dig_P1 = (buf[7] << 8) | buf[6];
	calib.dig_P2 = (buf[9] << 8) | buf[8];
	calib.dig_P3 = (buf[11] << 8) | buf[10];
	calib.dig_P4 = (buf[13] << 8) | buf[12];
	calib.dig_P5 = (buf[15] << 8) | buf[14];
	calib.dig_P6 = (buf[17] << 8) | buf[16];
	calib.dig_P7 = (buf[19] << 8) | buf[18];
	calib.dig_P8 = (buf[21] << 8) | buf[20];
	calib.dig_P9 = (buf[23] << 8) | buf[22];

	bme280_read_regs(0xA1, &calib.dig_H1, 1);

	uint8_t buf2[7];
	bme280_read_regs(0xE1, buf2, 7);
	calib.dig_H2 = (buf2[1] << 8) | buf2[0];
	calib.dig_H3 = buf2[2];
	calib.dig_H4 = (buf2[3] << 4) | (buf2[4] & 0x0F);
	calib.dig_H5 = (buf2[5] << 4) | (buf2[4] >> 4);
	calib.dig_H6 = (int8_t)buf2[6];
}

// Формули компенсації Bosch для розрахунку фізичних величин
float compensate_temperature(int32_t adc_T) {
	int32_t var1, var2;
	var1 = ((((adc_T >> 3) - ((int32_t)calib.dig_T1 << 1))) *
			((int32_t)calib.dig_T2)) >>
		   11;
	var2 = (((((adc_T >> 4) - ((int32_t)calib.dig_T1)) *
			  ((adc_T >> 4) - ((int32_t)calib.dig_T1))) >>
			 12) *
			((int32_t)calib.dig_T3)) >>
		   14;
	calib.t_fine = var1 + var2;
	return (float)((calib.t_fine * 5 + 128) >> 8) / 100.0f;
}

float compensate_pressure(int32_t adc_P) {
	int64_t var1, var2, p;
	var1 = ((int64_t)calib.t_fine) - 128000;
	var2 = var1 * var1 * (int64_t)calib.dig_P6;
	var2 = var2 + ((var1 * (int64_t)calib.dig_P5) << 17);
	var2 = var2 + ((int64_t)calib.dig_P4 << 35);
	var1 = ((var1 * var1 * (int64_t)calib.dig_P3) >> 8) +
		   ((var1 * (int64_t)calib.dig_P2) << 12);
	var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib.dig_P1) >> 33;
	if (var1 == 0)
		return 0;
	p = 1048576 - adc_P;
	p = (((p << 31) - var2) * 3125) / var1;
	var1 = (((int64_t)calib.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
	var2 = (((int64_t)calib.dig_P8) * p) >> 19;
	p = ((p + var1 + var2) >> 8) + ((int64_t)calib.dig_P7 << 4);
	return (float)p / 256.0f / 100.0f;
}

float compensate_humidity(int32_t adc_H) {
	int32_t v_x1_u32r;
	v_x1_u32r = (calib.t_fine - ((int32_t)76800));
	v_x1_u32r = (((((adc_H << 14) - (((int32_t)calib.dig_H4) << 20) -
					(((int32_t)calib.dig_H5) * v_x1_u32r)) +
				   ((int32_t)16384)) >>
				  15) *
				 (((((((v_x1_u32r * ((int32_t)calib.dig_H6)) >> 10) *
					  (((v_x1_u32r * ((int32_t)calib.dig_H3)) >> 11) +
					   ((int32_t)32768))) >>
					 10) +
					((int32_t)2097152)) *
					   ((int32_t)calib.dig_H2) +
				   8192) >>
				  14));
	v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) *
							   ((int32_t)calib.dig_H1)) >>
							  4));
	v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
	v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
	return (float)(v_x1_u32r >> 12) / 1024.0f;
}

void spi_and_devcie_init() {
	char *TAG = "BME280_SPI";
	esp_err_t ret;

	// Конфігурація головної шини SPI для нових пінів
	spi_bus_config_t buscfg = {.miso_io_num = PIN_NUM_MISO,
							   .mosi_io_num = PIN_NUM_MOSI,
							   .sclk_io_num = PIN_NUM_CLK,
							   .quadwp_io_num = -1,
							   .quadhd_io_num = -1,
							   .max_transfer_sz = 32};

	// Ініціалізація шини SPI2_HOST (рідна шина для ESP32-S3)
	ret = spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);
	ESP_ERROR_CHECK(ret);

	// Конфігурація інтерфейсу пристрою BME280 (СSB на GPIO 5)
	spi_device_interface_config_t devcfg = {
		.clock_speed_hz = 1 * 1000 * 1000, // Частота SPI 1 МГц
		.mode = 0,						   // SPI Mode 0
		.spics_io_num = PIN_NUM_CS,		   // CS на GPIO 5
		.queue_size = 7,
	};

	ret = spi_bus_add_device(SPI2_HOST, &devcfg, &spi);
	ESP_ERROR_CHECK(ret);

	// Читання Chip ID для верифікації
	uint8_t chip_id = 0;
	bme280_read_regs(BME280_REG_CHIPID, &chip_id, 1);
	if (chip_id != 0x60) {
		ESP_LOGE(TAG,
				 "Помилка! Датчик не знайдено, зчитаний ID: 0x%02X (очікувався "
				 "0x60)",
				 chip_id);
		return;
	}
	ESP_LOGI(TAG, "Датчик BME280 успішно підключено! ID: 0x%02X", chip_id);

	bme280_read_calibration();

	// Ініціалізація режимів датчика
	bme280_write_reg(BME280_REG_CTRL_HUM, 0x01); // Вологість x1
	bme280_write_reg(BME280_REG_CTRL_MEAS,
					 0x27); // Температура x1, Тиск x1, Normal режим
	bme280_write_reg(BME280_REG_CONFIG,
					 0xA0); // Фільтр вимкнено, Standby 1000ms
}

bme280_data bme_280_read_data() {
	uint8_t data_raw[8];
	bme280_data data;
	bme280_read_regs(BME280_REG_DATA, data_raw, 8);

	// Парсинг сирих значень
	int32_t adc_P =
		(data_raw[0] << 12) | (data_raw[1] << 4) | (data_raw[2] >> 4);
	int32_t adc_T =
		(data_raw[3] << 12) | (data_raw[4] << 4) | (data_raw[5] >> 4);
	int32_t adc_H = (data_raw[6] << 8) | data_raw[7];

	// Математичний розрахунок реальних показників
	data.temperature = compensate_temperature(adc_T);
	data.pressure = compensate_pressure(adc_P);
	data.humidity = compensate_humidity(adc_H);

	// Виведення результату

	return data;
}