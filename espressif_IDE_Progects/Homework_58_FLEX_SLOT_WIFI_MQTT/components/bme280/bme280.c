#include "include/bme280.h"
#include "esp_log.h"
#include <math.h>

#define TAG "BME280"

static i2c_port_t _i2c_num = I2C_NUM_0;

void bme280_init(i2c_port_t i2c_num) {
	_i2c_num = i2c_num;
	// У реальному проєкті тут перевіряється чіп-ід (chip_id == 0x60 для BME280)
	ESP_LOGI(TAG, "BME280 initialized on I2C port %d", _i2c_num);
}

bool bme280_read_data(bme280_data_t *data) {
	// Демо-імітація динамічної зміни показників датчика
	static float base_temp = 21.2f;
	base_temp += 0.05f;

	data->temperature = base_temp;
	data->humidity = 60.0f + (sinf(base_temp) * 2.0f);
	data->pressure = 762.0f + (cosf(base_temp) * 1.5f);

	return true;
}