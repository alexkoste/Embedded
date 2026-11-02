#include "include/network_manager.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_now.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TAG "NET_MGR"
#define WIFI_SSID "NameOfNetwork"
#define WIFI_PASS "NetworkPass"
#define MAX_DEVICES 7
#define MAX_HISTORY_RECORDS 50

typedef struct {
	char name[32];
	float temperature;
	float pressure;
	float humidity;
	bool active;
} __attribute__((packed)) device_node_t;

typedef struct {
	float temperature;
	float humidity;
	float pressure;
	int time_sec;
} history_entry_t;

typedef struct {
	char name[32];
	float temp_min;
	float temp_max;
	float hum_min;
	float hum_max;
	history_entry_t history[MAX_HISTORY_RECORDS];
	int history_count;
	int history_head;
	int total_samples;
} device_stats_t;

static device_node_t devices[MAX_DEVICES];
static device_stats_t stats[MAX_DEVICES];
static int device_count = 0;
static char server_ip_str[16] = "0.0.0.0";

// Надійний пошук або створення статистики виключно за ім'ям пристрою
static device_stats_t *get_or_create_stats(const char *name) {
	// 1. Шукаємо вже існуючий запис за ім'ям
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (stats[i].name[0] != '\0' && strcmp(stats[i].name, name) == 0) {
			return &stats[i];
		}
	}
	// 2. Якщо не знайдено, шукаємо вільний слот
	for (int i = 0; i < MAX_DEVICES; i++) {
		if (stats[i].name[0] == '\0') {
			strcpy(stats[i].name, name);
			stats[i].temp_min = 999.0f;
			stats[i].temp_max = -999.0f;
			stats[i].hum_min = 999.0f;
			stats[i].hum_max = -999.0f;
			stats[i].history_count = 0;
			stats[i].history_head = 0;
			stats[i].total_samples = 0;
			return &stats[i];
		}
	}
	return &stats[0]; // Аварійний фолбек
}

// Збереження історії та розрахунок мін/макс
void update_device_history(const char *name, float temp, float hum,
						   float pres) {
	device_stats_t *st = get_or_create_stats(name);

	if (st->temp_min > 900.0f || temp < st->temp_min)
		st->temp_min = temp;
	if (temp > st->temp_max)
		st->temp_max = temp;
	if (st->hum_min > 900.0f || hum < st->hum_min)
		st->hum_min = hum;
	if (hum > st->hum_max)
		st->hum_max = hum;

	st->total_samples++;

	int idx = st->history_head;
	st->history[idx].temperature = temp;
	st->history[idx].humidity = hum;
	st->history[idx].pressure = pres;
	st->history[idx].time_sec = st->total_samples * 5;

	st->history_head = (st->history_head + 1) % MAX_HISTORY_RECORDS;
	if (st->history_count < MAX_HISTORY_RECORDS) {
		st->history_count++;
	}
}

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
	if (idx == -1 && device_count < MAX_DEVICES) {
		idx = device_count++;
		strcpy(devices[idx].name, incoming->name);
	}

	if (idx != -1) {
		devices[idx] = *incoming;
		devices[idx].active = true;
		get_or_create_stats(
			devices[idx].name); // Гарантуємо ініціалізацію статистики
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

	get_or_create_stats("Base");
}

// Фонова задача збору історії для ВСІХ активних пристроїв кожні 5 секунд
static void history_task(void *pvParameters) {
	while (1) {
		vTaskDelay(pdMS_TO_TICKS(5000));
		for (int i = 0; i < device_count; i++) {
			if (devices[i].active) {
				update_device_history(devices[i].name, devices[i].temperature,
									  devices[i].humidity, devices[i].pressure);
			}
		}
	}
}

// Експорт у CSV
static esp_err_t csv_get_handler(httpd_req_t *req) {
	httpd_resp_set_type(req, "text/csv");
	httpd_resp_set_hdr(req, "Content-Disposition",
					   "attachment; filename=climate_history.csv");

	httpd_resp_send_chunk(
		req, "Device,Time(s),Temperature(C),Humidity(%),Pressure(hPa)\n",
		HTTPD_RESP_USE_STRLEN);
	char line[128];

	for (int i = 0; i < device_count; i++) {
		device_stats_t *st = get_or_create_stats(devices[i].name);
		for (int j = 0; j < st->history_count; j++) {
			int idx = (st->history_head - st->history_count + j +
					   MAX_HISTORY_RECORDS) %
					  MAX_HISTORY_RECORDS;
			snprintf(line, sizeof(line), "%s,%d,%.2f,%.2f,%.2f\n", st->name,
					 st->history[idx].time_sec, st->history[idx].temperature,
					 st->history[idx].humidity, st->history[idx].pressure);
			httpd_resp_send_chunk(req, line, HTTPD_RESP_USE_STRLEN);
		}
	}
	httpd_resp_send_chunk(req, NULL, 0);
	return ESP_OK;
}

// JSON ендпоінт для асинхронного оновлення (/data)
static esp_err_t data_get_handler(httpd_req_t *req) {
	char *buf = malloc(12288); // Збільшено буфер під другий масив історії
	if (!buf) {
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}

	int offset = snprintf(buf, 12288, "[");
	for (int i = 0; i < device_count; i++) {
		device_stats_t *st = get_or_create_stats(devices[i].name);
		float t_min =
			(st->temp_min > 900.0f) ? devices[i].temperature : st->temp_min;
		float t_max =
			(st->temp_max < -900.0f) ? devices[i].temperature : st->temp_max;
		float h_min =
			(st->hum_min > 900.0f) ? devices[i].humidity : st->hum_min;
		float h_max =
			(st->hum_max < -900.0f) ? devices[i].humidity : st->hum_max;

		offset +=
			snprintf(buf + offset, 12288 - offset,
					 "{\"name\":\"%s\",\"temp\":%.2f,\"temp_min\":%.2f,\"temp_"
					 "max\":%.2f,"
					 "\"hum\":%.2f,\"hum_min\":%.2f,\"hum_max\":%.2f,\"press\":"
					 "%.2f,\"history\":[",
					 devices[i].name, devices[i].temperature, t_min, t_max,
					 devices[i].humidity, h_min, h_max, devices[i].pressure);

		for (int j = 0; j < st->history_count; j++) {
			int idx = (st->history_head - st->history_count + j +
					   MAX_HISTORY_RECORDS) %
					  MAX_HISTORY_RECORDS;
			offset += snprintf(buf + offset, 12288 - offset,
							   "{\"t\":\"%ds\",\"temp\":%.2f,\"hum\":%.2f}%s",
							   st->history[idx].time_sec,
							   st->history[idx].temperature,
							   st->history[idx].humidity,
							   (j < st->history_count - 1) ? "," : "");
		}
		offset += snprintf(buf + offset, 12288 - offset, "]}%s",
						   (i < device_count - 1) ? "," : "");
	}
	offset += snprintf(buf + offset, 12288 - offset, "]");

	httpd_resp_set_type(req, "application/json");
	httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
	free(buf);
	return ESP_OK;
}

// Головна вебсторінка з асинхронним оновленням
static esp_err_t root_get_handler(httpd_req_t *req) {
	size_t buf_size = 20480; // Збільшено буфер для двох графіків
	char *resp = malloc(buf_size);
	if (!resp) {
		httpd_resp_send_500(req);
		return ESP_FAIL;
	}

	int offset = snprintf(
		resp, buf_size,
		"<!DOCTYPE html><html><head><meta charset='utf-8'>"
		"<meta name='viewport' content='width=device-width, initial-scale=1'>"
		"<title>Кліматична станція</title>"
		"<script src='https://cdn.jsdelivr.net/npm/chart.js'></script>"
		"<style>"
		"body { font-family: 'Segoe UI', Tahoma, sans-serif; background: "
		"#f0f2f5; margin: 0; padding: 20px; color: #333; }"
		"h1 { text-align: center; color: #2c3e50; }"
		".container { max-width: 950px; margin: auto; }"
		".card { background: white; padding: 20px; margin-bottom: 20px; "
		"border-radius: 12px; box-shadow: 0 4px 12px rgba(0,0,0,0.05); }"
		".card h2 { margin-top: 0; color: #3498db; border-bottom: 2px solid "
		"#ecf0f1; padding-bottom: 10px; }"
		".data-grid { display: flex; justify-content: space-around; font-size: "
		"1.1em; margin-bottom: 15px; flex-wrap: wrap; }"
		".val { font-weight: bold; color: #2c3e50; }"
		".minmax { font-size: 0.85em; color: #7f8c8d; margin-top: 4px; }"
		".btn-container { text-align: center; margin-top: 30px; }"
		".btn { background: #2ecc71; color: white; padding: 12px 24px; "
		"text-decoration: none; border-radius: 6px; font-weight: bold; "
		"font-size: 1em; box-shadow: 0 2px 6px rgba(46,204,113,0.3); "
		"transition: 0.2s; }"
		".btn:hover { background: #27ae60; }"
		".chart-container { position: relative; width: 100%%; height: 220px; "
		"margin-bottom: 15px; }"
		"</style></head><body>"
		"<div class='container'>"
		"<h1>🌡️ Кліматична станція</h1>");

	for (int i = 0; i < device_count; i++) {
		device_stats_t *st = get_or_create_stats(devices[i].name);
		float t_min =
			(st->temp_min > 900.0f) ? devices[i].temperature : st->temp_min;
		float t_max =
			(st->temp_max < -900.0f) ? devices[i].temperature : st->temp_max;
		float h_min =
			(st->hum_min > 900.0f) ? devices[i].humidity : st->hum_min;
		float h_max =
			(st->hum_max < -900.0f) ? devices[i].humidity : st->hum_max;

		if (offset < (buf_size - 4000)) {
			offset += snprintf(
				resp + offset, buf_size - offset,
				"<div class='card'>"
				"<h2>📊 Пристрій: <span id='name_%d'>%s</span></h2>"
				"<div class='data-grid'>"
				"<div>Темп: <span class='val' id='temp_%d'>%.1f</span> °C<div "
				"class='minmax'>мін: <span id='tmin_%d'>%.1f</span> | макс: "
				"<span id='tmax_%d'>%.1f</span></div></div>"
				"<div>Вологість: <span class='val' id='hum_%d'>%.1f</span> "
				"%%<div class='minmax'>мін: <span id='hmin_%d'>%.1f</span> | "
				"макс: <span id='hmax_%d'>%.1f</span></div></div>"
				"<div>Тиск: <span class='val' id='pres_%d'>%.1f</span> "
				"hPa</div>"
				"</div>"
				"<h3>Графік температури (°C)</h3>"
				"<div class='chart-container'><canvas "
				"id='chart_temp_%d'></canvas></div>"
				"<h3>Графік вологості (%%)</h3>"
				"<div class='chart-container'><canvas "
				"id='chart_hum_%d'></canvas></div>"
				"</div>",
				i, devices[i].name, i, devices[i].temperature, i, t_min, i,
				t_max, i, devices[i].humidity, i, h_min, i, h_max, i,
				devices[i].pressure, i, i);
		}
	}

	offset +=
		snprintf(resp + offset, buf_size - offset,
				 "<div class='btn-container'>"
				 "<a href='/csv' class='btn'>📥 Завантажити історію (CSV)</a>"
				 "</div></div><script>"
				 "const tempCharts = {};"
				 "const humCharts = {};"
				 "window.onload = function() {");

	// Ініціалізація графіків температури та вологості
	for (int i = 0; i < device_count && i < MAX_DEVICES; i++) {
		device_stats_t *st = get_or_create_stats(devices[i].name);
		if (offset < (buf_size - 4000)) {
			// Графік температури
			offset += snprintf(resp + offset, buf_size - offset,
							   "tempCharts[%d] = new "
							   "Chart(document.getElementById('chart_temp_%d')."
							   "getContext('2d'), {"
							   "  type: 'line',"
							   "  data: { labels: [",
							   i, i);
			for (int j = 0; j < st->history_count; j++) {
				int idx = (st->history_head - st->history_count + j +
						   MAX_HISTORY_RECORDS) %
						  MAX_HISTORY_RECORDS;
				offset += snprintf(resp + offset, buf_size - offset, "'%ds'%s",
								   st->history[idx].time_sec,
								   (j < st->history_count - 1) ? "," : "");
			}
			offset +=
				snprintf(resp + offset, buf_size - offset,
						 "], datasets: [{ label: 'Температура (°C)', data: [");
			for (int j = 0; j < st->history_count; j++) {
				int idx = (st->history_head - st->history_count + j +
						   MAX_HISTORY_RECORDS) %
						  MAX_HISTORY_RECORDS;
				offset += snprintf(resp + offset, buf_size - offset, "%.2f%s",
								   st->history[idx].temperature,
								   (j < st->history_count - 1) ? "," : "");
			}
			offset += snprintf(
				resp + offset, buf_size - offset,
				"], borderColor: '#e74c3c', backgroundColor: 'rgba(231, 76, "
				"60, 0.1)', tension: 0.2, fill: true }] },"
				"  options: { responsive: true, maintainAspectRatio: false, "
				"scales: { y: { min: 0, max: 50 } } }"
				"});");

			// Графік вологості
			offset += snprintf(resp + offset, buf_size - offset,
							   "humCharts[%d] = new "
							   "Chart(document.getElementById('chart_hum_%d')."
							   "getContext('2d'), {"
							   "  type: 'line',"
							   "  data: { labels: [",
							   i, i);
			for (int j = 0; j < st->history_count; j++) {
				int idx = (st->history_head - st->history_count + j +
						   MAX_HISTORY_RECORDS) %
						  MAX_HISTORY_RECORDS;
				offset += snprintf(resp + offset, buf_size - offset, "'%ds'%s",
								   st->history[idx].time_sec,
								   (j < st->history_count - 1) ? "," : "");
			}
			offset +=
				snprintf(resp + offset, buf_size - offset,
						 "], datasets: [{ label: 'Вологість (%%)', data: [");
			for (int j = 0; j < st->history_count; j++) {
				int idx = (st->history_head - st->history_count + j +
						   MAX_HISTORY_RECORDS) %
						  MAX_HISTORY_RECORDS;
				offset += snprintf(resp + offset, buf_size - offset, "%.2f%s",
								   st->history[idx].humidity,
								   (j < st->history_count - 1) ? "," : "");
			}
			offset += snprintf(
				resp + offset, buf_size - offset,
				"], borderColor: '#3498db', backgroundColor: 'rgba(52, 152, "
				"219, 0.1)', tension: 0.2, fill: true }] },"
				"  options: { responsive: true, maintainAspectRatio: false, "
				"scales: { y: { min: 0, max: 100 } } }"
				"});");
		}
	}

	// Асинхронне оновлення обох графіків і цифрових значень через Fetch API
	// кожні 5 секунд
	offset += snprintf(
		resp + offset, buf_size - offset,
		"setInterval(async () => {"
		"  try {"
		"    let res = await fetch('/data');"
		"    let json = await res.json();"
		"    json.forEach((dev, i) => {"
		"      if(document.getElementById('temp_' + i)) {"
		"        document.getElementById('temp_' + i).innerText = "
		"dev.temp.toFixed(1);"
		"        document.getElementById('tmin_' + i).innerText = "
		"dev.temp_min.toFixed(1);"
		"        document.getElementById('tmax_' + i).innerText = "
		"dev.temp_max.toFixed(1);"
		"        document.getElementById('hum_' + i).innerText = "
		"dev.hum.toFixed(1);"
		"        document.getElementById('hmin_' + i).innerText = "
		"dev.hum_min.toFixed(1);"
		"        document.getElementById('hmax_' + i).innerText = "
		"dev.hum_max.toFixed(1);"
		"        document.getElementById('pres_' + i).innerText = "
		"dev.press.toFixed(1);"
		"        if(tempCharts[i]) {"
		"          tempCharts[i].data.labels = dev.history.map(h => h.t);"
		"          tempCharts[i].data.datasets[0].data = dev.history.map(h => "
		"h.temp);"
		"          tempCharts[i].update('none');"
		"        }"
		"        if(humCharts[i]) {"
		"          humCharts[i].data.labels = dev.history.map(h => h.t);"
		"          humCharts[i].data.datasets[0].data = dev.history.map(h => "
		"h.hum);"
		"          humCharts[i].update('none');"
		"        }"
		"      }"
		"    });"
		"  } catch(e) { console.log('Fetch error:', e); }"
		"}, 5000);"
		"};</script></body></html>");

	httpd_resp_send(req, resp, HTTPD_RESP_USE_STRLEN);
	free(resp);
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

	strcpy(devices[0].name, "Base");
	devices[0].temperature = 0.0f;
	devices[0].pressure = 1013.25f;
	devices[0].humidity = 50.0f;
	devices[0].active = true;
	device_count = 1;
	get_or_create_stats("Base");

	wifi_config_t wifi_config = {
		.sta = {.ssid = WIFI_SSID, .password = WIFI_PASS}};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	uint8_t base_mac[6];
	esp_read_mac(base_mac, ESP_MAC_WIFI_STA);
	ESP_LOGI(TAG, "==================================================");
	ESP_LOGI(TAG, " GATEWAY MAC ADDRESS: %02X:%02X:%02X:%02X:%02X:%02X",
			 base_mac[0], base_mac[1], base_mac[2], base_mac[3], base_mac[4],
			 base_mac[5]);
	ESP_LOGI(TAG, "==================================================");

	ESP_ERROR_CHECK(esp_wifi_connect());

	ESP_ERROR_CHECK(esp_now_init());
	ESP_ERROR_CHECK(esp_now_register_recv_cb(esp_now_recv_cb));

	// Запуск фонової задачі збору історії для всіх активних пристроїв кожні 5
	// секунд
	xTaskCreate(history_task, "history_task", 4096, NULL, 5, NULL);

	httpd_config_t config = HTTPD_DEFAULT_CONFIG();
	httpd_handle_t server = NULL;
	if (httpd_start(&server, &config) == ESP_OK) {
		httpd_uri_t root = {
			.uri = "/", .method = HTTP_GET, .handler = root_get_handler};
		httpd_uri_t data_uri = {
			.uri = "/data", .method = HTTP_GET, .handler = data_get_handler};
		httpd_uri_t csv = {
			.uri = "/csv", .method = HTTP_GET, .handler = csv_get_handler};

		httpd_register_uri_handler(server, &root);
		httpd_register_uri_handler(server, &data_uri);
		httpd_register_uri_handler(server, &csv);
	}
}