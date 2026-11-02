#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "esp_sleep.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "include/bme280.h"
#include "include/ds1306.h"
#include "include/ssd1306.h"

#define TAG "LOG_SYSTEM"

#define I2C_MASTER_SDA_IO 8
#define I2C_MASTER_SCL_IO 9
#define I2C_MASTER_NUM I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000

#define UART_NUM UART_NUM_0
#define BUF_SIZE 1024

#define LOG_INTERVAL_MIN 1
#define FLASH_SECTOR_SIZE 4096

typedef struct __attribute__((packed)) {
	uint32_t timestamp;
	int16_t temperature;
	uint16_t humidity;
	uint16_t pressure;
} log_entry_t;

typedef struct {
	float temp;
	float hum;
	float press;
	uint32_t timestamp;
} sensor_data_t;

typedef enum {
	CMD_UNKNOWN = 0,
	CMD_STATUS,
	CMD_LOG_ON,
	CMD_LOG_OFF,
	CMD_FLUSH
} cmd_type_t;

static cmd_type_t parse_command(const char *cmd) {
	if (strcmp(cmd, "status") == 0)
		return CMD_STATUS;
	if (strcmp(cmd, "log on") == 0)
		return CMD_LOG_ON;
	if (strcmp(cmd, "log off") == 0)
		return CMD_LOG_OFF;
	if (strcmp(cmd, "flush") == 0)
		return CMD_FLUSH;
	return CMD_UNKNOWN;
}

static SemaphoreHandle_t i2c_mutex = NULL;
static QueueHandle_t sensor_queue = NULL;
static QueueHandle_t flash_queue = NULL;

static bool logging_enabled = true;
static const esp_partition_t *log_partition = NULL;
static uint32_t current_flash_offset = 0;
static uint8_t sector_buffer[FLASH_SECTOR_SIZE];
static size_t sector_buf_index = 0;

static void sensor_task(void *arg);
static void display_task(void *arg);
static void uart_task(void *arg);
static void manager_task(void *arg);
static void flash_writer_task(void *arg);

void app_main(void) {
	ESP_LOGI(TAG, "Starting Modular ESP32-S3 Logging System");

	i2c_config_t conf = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_MASTER_SDA_IO,
		.scl_io_num = I2C_MASTER_SCL_IO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_MASTER_FREQ_HZ,
	};
	i2c_param_config(I2C_MASTER_NUM, &conf);
	i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);

	i2c_mutex = xSemaphoreCreateMutex();
	sensor_queue = xQueueCreate(1, sizeof(sensor_data_t));
	flash_queue = xQueueCreate(5, sizeof(log_entry_t));

	// Ініціалізація компонентів за допомогою мютекса шини
	if (xSemaphoreTake(i2c_mutex, portMAX_DELAY) == pdTRUE) {
		ssd1306_init(I2C_MASTER_NUM);
		bme280_init(I2C_MASTER_NUM, 0x76);
		ds1306_init();
		xSemaphoreGive(i2c_mutex);
	}

	log_partition = esp_partition_find_first(
		ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "logdata");

	const uart_config_t uart_config = {.baud_rate = 115200,
									   .data_bits = UART_DATA_8_BITS,
									   .parity = UART_PARITY_DISABLE,
									   .stop_bits = UART_STOP_BITS_1,
									   .flow_ctrl = UART_HW_FLOWCTRL_DISABLE};
	uart_param_config(UART_NUM, &uart_config);
	uart_driver_install(UART_NUM, BUF_SIZE * 2, 0, 0, NULL, 0);

	// Створення 5 задач FreeRTOS
	xTaskCreate(sensor_task, "SensorTask", 2048, NULL, 3, NULL);
	xTaskCreate(display_task, "DisplayTask", 4096, NULL, 2, NULL);
	xTaskCreate(uart_task, "UARTTask", 2048, NULL, 1, NULL);
	xTaskCreate(manager_task, "ManagerTask", 2048, NULL, 4, NULL);
	xTaskCreate(flash_writer_task, "FlashWriterTask", 2048, NULL, 2, NULL);
}

static void sensor_task(void *arg) {
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(1000);
	xLastWakeTime = xTaskGetTickCount();

	while (1) {
		sensor_data_t data;
		if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
			if (bme280_read_data(I2C_MASTER_NUM, &data.temp, &data.hum,
								 &data.press) == ESP_OK &&
				ds1306_get_timestamp(&data.timestamp) == ESP_OK) {
				xQueueOverwrite(sensor_queue, &data);
			}
			xSemaphoreGive(i2c_mutex);
		}
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
	}
}

static void display_task(void *arg) {
	sensor_data_t data;
	while (1) {
		if (xQueueReceive(sensor_queue, &data, portMAX_DELAY) == pdTRUE) {
			char line1[32], line2[32], line3[32], line4[32];
			snprintf(line1, sizeof(line1), "Time: %lu",
					 (unsigned long)data.timestamp);
			snprintf(line2, sizeof(line2), "Temp:  %.2f C", data.temp);
			snprintf(line3, sizeof(line3), "Hum:   %.2f %%", data.hum);
			snprintf(line4, sizeof(line4), "Press: %.1f mmHg", data.press);

			if (xSemaphoreTake(i2c_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
				ssd1306_clear();
				ssd1306_draw_string(0, 0, line1);
				ssd1306_draw_string(0, 2, line2);
				ssd1306_draw_string(0, 4, line3);
				ssd1306_draw_string(0, 6, line4);
				ssd1306_refresh();
				xSemaphoreGive(i2c_mutex);
			}
		}
	}
}

static void manager_task(void *arg) {
	sensor_data_t data;
	TickType_t last_flash_time = xTaskGetTickCount();
	const TickType_t flash_interval =
		pdMS_TO_TICKS(LOG_INTERVAL_MIN * 60 * 1000);

	while (1) {
		if (xQueuePeek(sensor_queue, &data, pdMS_TO_TICKS(1000)) == pdTRUE) {
			if (logging_enabled && log_partition != NULL) {
				if ((xTaskGetTickCount() - last_flash_time) >= flash_interval) {
					log_entry_t entry = {.timestamp = data.timestamp,
										 .temperature =
											 (int16_t)(data.temp * 100),
										 .humidity = (uint16_t)(data.hum * 100),
										 .pressure = (uint16_t)(data.press)};
					xQueueSend(flash_queue, &entry, 0);
					last_flash_time = xTaskGetTickCount();
				}
			}
		}

		esp_sleep_enable_timer_wakeup(1000000);
		esp_light_sleep_start();
	}
}

static void flash_writer_task(void *arg) {
	log_entry_t entry;
	ESP_LOGI(TAG, "Sector written to flash");

	while (1) {
		if (xQueueReceive(flash_queue, &entry, portMAX_DELAY) == pdTRUE) {
			if (log_partition == NULL)
				continue;

			if (current_flash_offset + sizeof(log_entry_t) >
				log_partition->size) {
				logging_enabled = false;
				ESP_LOGE(TAG, "FLASH FULL! Logging stopped.");
				continue;
			}

			memcpy(&sector_buffer[sector_buf_index], &entry,
				   sizeof(log_entry_t));
			sector_buf_index += sizeof(log_entry_t);

			if (sector_buf_index >= FLASH_SECTOR_SIZE) {
				esp_err_t err =
					esp_partition_write(log_partition, current_flash_offset,
										sector_buffer, FLASH_SECTOR_SIZE);
				if (err == ESP_OK) {
					ESP_LOGI(TAG, "Sector written to flash at offset: 0x%lx",
							 current_flash_offset);
					current_flash_offset += FLASH_SECTOR_SIZE;
				} else {
					ESP_LOGE(TAG, "Failed to write to flash!");
				}
				sector_buf_index = 0;
				memset(sector_buffer, 0, FLASH_SECTOR_SIZE);
			}
		}
	}
}

static void uart_task(void *arg) {
	uint8_t data[128];
	while (1) {
		int len = uart_read_bytes(UART_NUM, data, sizeof(data) - 1,
								  pdMS_TO_TICKS(100));
		if (len > 0) {
			data[len] = '\0';
			char *cmd = (char *)data;
			cmd[strcspn(cmd, "\r\n")] = 0;

			switch (parse_command(cmd)) {
			case CMD_STATUS:
				ESP_LOGI(TAG, "STATUS: Logging = %s, Offset = %lu/%ld",
						 logging_enabled ? "ON" : "OFF", current_flash_offset,
						 log_partition ? log_partition->size : 0);
				break;

			case CMD_LOG_ON:
				logging_enabled = true;
				ESP_LOGI(TAG, "Logging enabled.");
				break;

			case CMD_LOG_OFF:
				logging_enabled = false;
				ESP_LOGI(TAG, "Logging disabled.");
				break;

			case CMD_FLUSH:
				if (sector_buf_index > 0 && log_partition != NULL) {
					esp_partition_write(log_partition, current_flash_offset,
										sector_buffer, FLASH_SECTOR_SIZE);
					current_flash_offset += FLASH_SECTOR_SIZE;
					sector_buf_index = 0;
					ESP_LOGI(TAG, "Buffer flushed to flash.");
				} else {
					ESP_LOGI(TAG, "Buffer is empty.");
				}
				break;

			case CMD_UNKNOWN:
			default:
				ESP_LOGW(TAG, "Unknown command: %s", cmd);
				break;
			}
		}
	}
}