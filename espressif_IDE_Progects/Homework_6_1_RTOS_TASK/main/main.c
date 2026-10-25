#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>

#define LED1_PIN GPIO_NUM_4
#define LED2_PIN GPIO_NUM_5
#define LED3_PIN GPIO_NUM_6

// Тег для виведення логів в Serial Monitor
static const char *TAG = "LED_TASKS";

// Прототипи задач FreeRTOS
static void led1_task(void *pvParameters);
static void led2_task(void *pvParameters);
static void led3_task(void *pvParameters);

void app_main(void) {
	ESP_LOGI(TAG, "Ініціалізація пінів світлодіодів...");

	// Налаштування пінів як виходів
	gpio_config_t io_conf = {
		.intr_type = GPIO_INTR_DISABLE,
		.mode = GPIO_MODE_OUTPUT,
		.pin_bit_mask =
			(1ULL << LED1_PIN) | (1ULL << LED2_PIN) | (1ULL << LED3_PIN),
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_DISABLE,
	};
	gpio_config(&io_conf);

	ESP_LOGI(TAG, "Створення задач FreeRTOS...");

	// Створення задач (xTaskCreate)
	// Параметри: функція, назва, розмір стеку, параметри, пріоритет, дескриптор
	xTaskCreate(led1_task, "led1_task", 2048, NULL, 5, NULL);
	xTaskCreate(led2_task, "led2_task", 2048, NULL, 5, NULL);
	xTaskCreate(led3_task, "led3_task", 2048, NULL, 5, NULL);

	while (1) {
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}

static void led1_task(void *pvParameters) {
	uint8_t state = 0;
	while (1) {
		state = !state;
		gpio_set_level(LED1_PIN, state);
		vTaskDelay(pdMS_TO_TICKS(200));
	}
}

static void led2_task(void *pvParameters) {
	uint8_t state = 0;
	while (1) {
		state = !state;
		gpio_set_level(LED2_PIN, state);
		vTaskDelay(pdMS_TO_TICKS(500));
	}
}

static void led3_task(void *pvParameters) {
	uint8_t state = 0;
	while (1) {
		state = !state;
		gpio_set_level(LED3_PIN, state);
		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}