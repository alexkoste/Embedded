#include "include/network_manager.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include <string.h>

#define TAG "NODE_NET"
#define NODE_NAME "Node_2"

// MAC-адреса головного модулю (Gateway). Замініть на реальну MAC-адресу вашого
// головного ESP32-S3!
static uint8_t gateway_mac[6] = {0xAC, 0xA7, 0x04, 0xE1, 0x0D, 0xEC};
static uint8_t channel = 9;

typedef struct {
	char name[32];
	float temperature;
	float pressure;
	float humidity;
	bool active;
} __attribute__((packed)) payload_t;

void network_init(void) {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_ERROR_CHECK(esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE));

	ESP_ERROR_CHECK(esp_now_init());

	// Додавання головного модуля як піра
	esp_now_peer_info_t peerInfo = {};

	memcpy(peerInfo.peer_addr, gateway_mac, 6);
	peerInfo.channel = channel; // Вказуємо канал явно!
	peerInfo.encrypt = false;

	if (esp_now_add_peer(&peerInfo) != ESP_OK) {
		ESP_LOGE(TAG, "Failed to add ESP-NOW gateway peer");
	} else {
		ESP_LOGI(TAG, "Gateway peer added successfully");
	}
}

void network_send_data(sensor_data_t *data) {
	payload_t payload;
	strcpy(payload.name, NODE_NAME);
	payload.temperature = data->temperature;
	payload.pressure = data->pressure;
	payload.humidity = data->humidity;
	payload.active = true;

	esp_err_t result =
		esp_now_send(gateway_mac, (uint8_t *)&payload, sizeof(payload));
	if (result == ESP_OK) {
		ESP_LOGI(TAG, "ESP-NOW data sent successfully");
	} else {
		ESP_LOGW(TAG, "Error sending ESP-NOW data");
	}
}