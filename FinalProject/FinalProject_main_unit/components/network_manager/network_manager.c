#include "include/network_manager.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_wifi.h"
#include <stdio.h>
#include <string.h>

#define TAG "NET_MGR"
#define WIFI_SSID "Klumba"
#define WIFI_PASS "HondaCBR600F4i"
#define MAX_DEVICES 7

typedef struct {
	char name[32];
	float temperature;
	float pressure;
	float humidity;
	bool active;
} __attribute__((packed)) device_node_t;

static device_node_t devices[MAX_DEVICES];
static int device_count = 0;
static char server_ip_str[16] = "0.0.0.0";

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
							   int32_t event_id, void *event_data) {
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		esp_wifi_connect();
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
		sprintf(server_ip_str, IPSTR, IP2STR(&event->ip_info.ip));
		ESP_LOGI(TAG, "WEB SERVER STARTED: http://%s", server_ip_str);
	}
}

static void esp_now_recv_cb(const struct esp_now_recv_info *recv_info,
							const uint8_t *data, int data_len) {
	if (data_len != sizeof(device_node_t))
		return;
	device_node_t *incoming = (device_node_t *)data;

	int idx = -1;
	for (int i = 0; i < device_count; i++) {
		if (strcmp(devices[i].name, incoming->name) == 0) {
			idx = i;
			break;
		}
	}
	if (idx == -1 && device_count < MAX_DEVICES)
		idx = device_count++;

	if (idx != -1) {
		devices[idx] = *incoming;
		devices[idx].active = true;
	}
}

int get_total_devices(void) { return device_count > 0 ? device_count : 1; }

void get_device_info(int index, char *name, float *temp, float *pres,
					 float *hum, bool *active) {
	if (index >= 0 && index < device_count) {
		strcpy(name, devices[index].name);
		*temp = devices[index].temperature;
		*pres = devices[index].pressure;
		*hum = devices[index].humidity;
		*active = devices[index].active;
	} else {
		strcpy(name, "Base");
		*temp = 0.0f;
		*pres = 0.0f;
		*hum = 0.0f;
		*active = true;
	}
}

void network_update_local_device(sensor_data_t *local_data) {
	if (device_count == 0) {
		strcpy(devices[0].name, "Base");
		device_count = 1;
	}
	devices[0].temperature = local_data->temperature;
	devices[0].pressure = local_data->pressure;
	devices[0].humidity = local_data->humidity;
	devices[0].active = true;
}

static esp_err_t root_get_handler(httpd_req_t *req) {
	char resp[1500];
	int offset =
		snprintf(resp, sizeof(resp),
				 "<html><head><meta charset='utf-8'><meta http-equiv='refresh' "
				 "content='5'>"
				 "<title>ESP32-S3 Base</title></head><body>"
				 "<h2>Кліматичний стенд - Моніторинг</h2>"
				 "<table border='1' "
				 "cellpadding='8'><tr><th>Пристрій</th><th>Температура</"
				 "th><th>Тиск</th><th>Вологість</th></tr>");

	for (int i = 0; i < device_count; i++) {
		offset += snprintf(resp + offset, sizeof(resp) - offset,
						   "<tr><td>%s</td><td>%.1f C</td><td>%.1f "
						   "hPa</td><td>%.1f %%</td></tr>",
						   devices[i].name, devices[i].temperature,
						   devices[i].pressure, devices[i].humidity);
	}
	strcat(resp, "</table></body></html>");
	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	return ESP_OK;
}

void network_init(void) {
	ESP_ERROR_CHECK(esp_netif_init());
	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
											   &wifi_event_handler, NULL));
	ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
											   &wifi_event_handler, NULL));

	wifi_config_t wifi_config = {
		.sta = {.ssid = WIFI_SSID, .password = WIFI_PASS}};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	uint8_t base_mac[6];
	// Для режиму STA використовуємо ESP_MAC_WIFI_STA
	esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
	ESP_LOGI(TAG, "==================================================");
	ESP_LOGI(TAG, " GATEWAY MAC ADDRESS: %02X:%02X:%02X:%02X:%02X:%02X",
			 base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4],
			 base_mac[5]);
	ESP_LOGI(TAG, "==================================================");

	ESP_ERROR_CHECK(esp_wifi_connect());

	ESP_ERROR_CHECK(esp_now_init());
	ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	httpd_handle_t server = NULL;
	if (httpd_start(&server, &config) == ESP_OK) {
		httpd_uri_t root = {
			.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
		httpd_register_uri_handler(server, &root);
	}
}