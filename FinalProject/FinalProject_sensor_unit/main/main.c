#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/display_manager.h"
#include "include/network_manager.h"
#include "include/sensor_manager.h"
#include "nvs_flash.h"
#include <stdio.h>

#define TAG "NODE_MAIN"

void app_main(void) {
	// Ініціалізація NVS (потрібна для Wi-Fi/ESP-NOW)
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
		ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	ESP_LOGI(TAG, "Initializing Sensor Manager...");
	sensor_init();

	ESP_LOGI(TAG, "Initializing Display Manager...");
	display_init();

	ESP_LOGI(TAG, "Initializing Network Manager (ESP-NOW Node)...");
	network_init();

	while (1) {
		// Збір даних з BME280
		sensor_data_t data = sensor_read();

		// Передача даних на головний модуль по ESP-NOW
		network_send_data(&data);

		// Оновлення локального екрана периферійного пристрою
		display_refresh(&data);

		vTaskDelay(pdMS_TO_TICKS(2000));
	}
}