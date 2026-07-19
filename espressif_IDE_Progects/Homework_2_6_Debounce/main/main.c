/*

TASK 2_6_1
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

#define BTN_PIN GPIO_NUM_5
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG ="BARE_METAL_ISR";
volatile uint32_t counter = 0;
volatile bool isEventTriggered = false;

static void IRAM_ATTR btn_heandler(void* arg){
	counter++;
	isEventTriggered=true;
}

void app_main(void)
{
	gpio_config_t conf = {
		.pin_bit_mask = (1ULL<<BTN_PIN),
		.intr_type = GPIO_INTR_NEGEDGE,
		.mode = GPIO_MODE_INPUT,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE
	};
	
	gpio_config(&conf);
	
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(BTN_PIN, btn_heandler, NULL);
	while(1){
		if(isEventTriggered){
			ESP_LOGI(TAG, "Interrupt! Total count: %lu", counter);
			isEventTriggered = false;
		}
		vTaskDelay(pdMS_TO_TICKS(10)); 
	}
}
*/

/*
TASK 2_6_2
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

#define BTN_PIN GPIO_NUM_5
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG ="BARE_METAL_ISR";
volatile uint32_t counter = 0;
static const uint32_t debounceDelay = 50;
volatile uint32_t lastPressedTime = 0;
volatile bool isEventTriggered = false;

static void IRAM_ATTR btn_heandler(void* arg){
	isEventTriggered=true;
}

void app_main(void)
{
	gpio_config_t conf = {
		.pin_bit_mask = (1ULL<<BTN_PIN),
		.intr_type = GPIO_INTR_NEGEDGE,
		.mode = GPIO_MODE_INPUT,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE
	};
	
	gpio_config(&conf);
	
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(BTN_PIN, btn_heandler, NULL);
	while(1){
		if(isEventTriggered){
			uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
			isEventTriggered = false;
			if(now-lastPressedTime>debounceDelay){
				lastPressedTime=now;
				counter++;
				ESP_LOGI(TAG, "Interrupt! Total count: %lu", counter);
			}
		}
	}
}
*/

/*
TASK 2_6_3
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

#define BTN_PIN GPIO_NUM_5
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG ="BARE_METAL_ISR";
volatile uint32_t counter = 0;
volatile bool isEventTriggered = false;

static void IRAM_ATTR btn_heandler(void* arg){
	isEventTriggered=true;
}

void app_main(void)
{
	gpio_config_t conf = {
		.pin_bit_mask = (1ULL<<BTN_PIN),
		.intr_type = GPIO_INTR_NEGEDGE,
		.mode = GPIO_MODE_INPUT,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE
	};
	
	gpio_config(&conf);
	
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(BTN_PIN, btn_heandler, NULL);
	while(1){
		if(isEventTriggered){
			isEventTriggered=false;
			if(gpio_get_level(BTN_PIN)==1){
				counter++;
				ESP_LOGI(TAG, "Interrupt! Total count: %lu", counter);
			}
		}
		vTaskDelay(pdMS_TO_TICKS(10)); 
	}
}
*/

//TASK 2_6_4

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/projdefs.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "hal/gpio_types.h"

#define BTN_PIN GPIO_NUM_5
#define ESP_INTR_FLAG_DEFAULT 0

static const char *TAG ="BARE_METAL_ISR";
volatile uint32_t counter = 0;
static const uint32_t debounceDelay = 50;
static const uint32_t pollingInterval = 10;
volatile uint32_t lastDebounceTime = 0;
volatile uint32_t lastPressedTime = 0;
volatile bool isEventTriggered = false;

static void IRAM_ATTR btn_heandler(void* arg){
	isEventTriggered=true;
}

void app_main(void)
{
	gpio_config_t conf = {
		.pin_bit_mask = (1ULL<<BTN_PIN),
		.intr_type = GPIO_INTR_NEGEDGE,
		.mode = GPIO_MODE_INPUT,
		.pull_down_en = GPIO_PULLDOWN_DISABLE,
		.pull_up_en = GPIO_PULLUP_ENABLE
	};
	
	gpio_config(&conf);
	
	gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);
	gpio_isr_handler_add(BTN_PIN, btn_heandler, NULL);
	while(1){
		uint32_t now = pdTICKS_TO_MS(xTaskGetTickCount());
		if(now-lastPressedTime>pollingInterval){
			lastPressedTime = now;
			uint32_t debounceNow = pdTICKS_TO_MS(xTaskGetTickCount());
			if(debounceNow - lastDebounceTime>debounceDelay)
			{
				lastDebounceTime = debounceNow;
				isEventTriggered = gpio_get_level(BTN_PIN)==1;
			}
			if(isEventTriggered){
				if(gpio_get_level(BTN_PIN)==0)
				{
					isEventTriggered = false;
					counter++;
					ESP_LOGI(TAG, "Interrupt! Total count: %lu", counter);
				}
			}
		}
		vTaskDelay(1);
	}
}