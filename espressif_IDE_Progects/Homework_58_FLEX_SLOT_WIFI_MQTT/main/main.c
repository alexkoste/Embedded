#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/bme280.h"
#include "include/mqtt_manager.h"
#include "include/ssd1306.h"
#include "include/wifi_manager.h"

#define TAG "MAIN_APP"

#define WIFI_SSID "NAME of NETWORK"
#define WIFI_PASS "PASS FOR NETWORK"
#define MQTT_BROKER_URI "mqtt://broker.hivemq.com"

#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_NUM I2C_NUM_0

#define TOPIC_PUB_TEMP "EMB1/TEST1/temp"
#define TOPIC_PUB_HUM "EMB1/TEST1/hum"
#define TOPIC_PUB_PRES "EMB1/TEST1/pres"
#define TOPIC_SUB_ALL "EMB1/#"

typedef struct {
	char student_name[32];
	float temp;
	float hum;
	float pres;
} display_state_t;

static display_state_t current_display = {
	.student_name = "TEST1", .temp = 0, .hum = 0, .pres = 0};

static void handle_incoming_mqtt_data(const char *topic, const char *data) {
	char prefix[16], student[32], sensor_type[16];
	int parsed =
		sscanf(topic, "%15[^/]/%31[^/]/%15s", prefix, student, sensor_type);

	if (parsed == 3 && strcmp(prefix, "EMB1") == 0) {
		float val = atof(data);

		strncpy(current_display.student_name, student,
				sizeof(current_display.student_name) - 1);
		if (strcmp(sensor_type, "temp") == 0)
			current_display.temp = val;
		else if (strcmp(sensor_type, "hum") == 0)
			current_display.hum = val;
		else if (strcmp(sensor_type, "pres") == 0)
			current_display.pres = val;

		printf("\n[CONSOLE] Student: %s | Sensor: %s | Value: %.2f\n", student,
			   sensor_type, val);
		ssd1306_update_telemetry("EMB1", current_display.student_name,
								 current_display.temp, current_display.pres,
								 current_display.hum);
	}
}

static void sensor_task(void *pvParameters) {
	bme280_data_t sensor_data;
	char payload[32];

	while (1) {
		if (bme280_read_data(&sensor_data)) {
			snprintf(payload, sizeof(payload), "%.1f", sensor_data.temperature);
			mqtt_manager_publish(TOPIC_PUB_TEMP, payload);

			snprintf(payload, sizeof(payload), "%.0f", sensor_data.humidity);
			mqtt_manager_publish(TOPIC_PUB_HUM, payload);

			snprintf(payload, sizeof(payload), "%.0f", sensor_data.pressure);
			mqtt_manager_publish(TOPIC_PUB_PRES, payload);

			ESP_LOGI(TAG, "Published -> T: %.1f, H: %.0f, P: %.0f",
					 sensor_data.temperature, sensor_data.humidity,
					 sensor_data.pressure);
		}
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

void app_main(void) {
	ESP_LOGI(TAG, "Starting Sensor Node Application on ESP32-S3...");

	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
		ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_MASTER_SDA_IO,
		.scl_io_num = I2C_MASTER_SCL_IO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = 400000,
	};
	i2c_param_config(I2C_MASTER_NUM, &conf);
	i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

	ssd1306_init(I2C_MASTER_NUM, I2C_MASTER_SDA_IO, I2C_MASTER_SCL_IO);
	ssd1306_clear();
	bme280_init(I2C_MASTER_NUM);

	wifi_init_sta(WIFI_SSID, WIFI_PASS);
	mqtt_manager_init(MQTT_BROKER_URI, TOPIC_SUB_ALL,
					  handle_incoming_mqtt_data);

	xTaskCreate(sensor_task, "sensor_task", 4096, NULL, 5, NULL);
}