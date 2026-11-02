#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "include/display_manager.h"
#include "include/network_manager.h"
#include "include/sensor_manager.h"
#include "nvs_flash.h"

#define TAG "MAIN"

void app_main(void) {
	ESP_ERROR_CHECK(nvs_flash_init());
	ESP_LOGI(TAG, "Starting Gateway...");

	sensor_init();
	display_init();
	network_init();

	while (1) {
		sensor_data_t data = sensor_read();
		network_update_local_device(&data);
		display_refresh();
	}
}