#include "include/bme280.h"
#include "esp_log.h"

static uint8_t bme_addr = 0x76;

esp_err_t bme280_init(i2c_port_t i2c_num, uint8_t addr) {
	bme_addr = addr;
	uint8_t data[2];
	// Перевірка Chip ID (має повернути 0x60 для BME280)
	esp_err_t err = i2c_master_write_read_device(
		i2c_num, bme_addr, (uint8_t[]){0xD0}, 1, data, 1, pdMS_TO_TICKS(100));
	if (err == ESP_OK && data[0] == 0x60) {
		// Налаштування контролю надвибірки (Temp x1, Press x1, Normal mode)
		i2c_master_write_to_device(
			i2c_num, bme_addr, (uint8_t[]){0xF2, 0x01}, 2,
			pdMS_TO_TICKS(100)); // Humidity oversampling x1
		i2c_master_write_to_device(
			i2c_num, bme_addr, (uint8_t[]){0xF4, 0x27}, 2,
			pdMS_TO_TICKS(100)); // Temp/Press oversampling & Normal mode
		return ESP_OK;
	}
	return ESP_FAIL;
}

esp_err_t bme280_read_data(i2c_port_t i2c_num, float *temp, float *hum,
						   float *press) {
	uint8_t rx_data[8];
	// Читання сирих даних з регістрів 0xF7 по 0xFE (pressure, temp, humidity)
	esp_err_t err =
		i2c_master_write_read_device(i2c_num, bme_addr, (uint8_t[]){0xF7}, 1,
									 rx_data, 8, pdMS_TO_TICKS(100));
	if (err != ESP_OK)
		return err;

	int32_t adc_p = (rx_data[0] << 12) | (rx_data[1] << 4) | (rx_data[2] >> 4);
	int32_t adc_t = (rx_data[3] << 12) | (rx_data[4] << 4) | (rx_data[5] >> 4);
	int32_t adc_h = (rx_data[6] << 8) | rx_data[7];

	// Спрощене переведення у фіксовані одиниці (для демонстрації реального
	// читання)
	*temp = (float)(adc_t / 5120.0f);
	*hum = (float)(adc_h / 512.0f);
	*press = (float)(adc_p / 16.0f * 0.0075f *
					 75.0f); // переведення в мм рт. ст. приблизно

	return ESP_OK;
}