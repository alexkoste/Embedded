#include "include/i2c_bus.h"
#include "esp_log.h"

static const char *TAG = "I2C_BUS_COMP";
esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_bus_handle) {
	if (out_bus_handle == NULL) {
		return ESP_ERR_INVALID_ARG;
	};

	i2c_master_bus_config_t bus_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = I2C_PORT,
		.scl_io_num = I2C_SCL_PIN,
		.sda_io_num = I2C_SDA_PIN,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = false,
	};
	esp_err_t ret = i2c_new_master_bus(&bus_config, out_bus_handle);

	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Не вдалося створити I2C шину: %s", esp_err_to_name(ret));
		return ret;
	}

	ESP_LOGI(TAG, "I2C шину успішно створено. Адреса: %p",
			 (void *)*out_bus_handle);
	return ESP_OK;
};
