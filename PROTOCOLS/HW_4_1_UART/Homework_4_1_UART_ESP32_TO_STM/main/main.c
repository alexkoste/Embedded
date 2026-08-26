#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

#define BTN_PIN GPIO_NUM_5
#define LED_PIN GPIO_NUM_4

#define UART_PORT UART_NUM_1
#define UART_TX_PIN 17
#define UART_RX_PIN 18
#define UART_BAUD 115200
#define UART_BUF_SIZE 256

#define DEBOUNCE_DELAY_MS 50

static const char *TAG = "UART_LINK";

static bool isLedOn = false;
static uint8_t isEnable = 0;
static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
	uint32_t gpio_num = (uint32_t)arg;
	xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
	isEnable = !isEnable;
}

static void button_task(void *arg) {
	uint32_t io_num;
	for (;;) {
		if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
			ESP_LOGI(TAG,
					 "GPIO[%" PRIu32 "] interrupt triggered! Button pressed.",
					 io_num);
		}
	}
}

void uart_link_init(void) {
	uart_config_t uart_config = {
		.baud_rate = UART_BAUD,
		.data_bits = UART_DATA_8_BITS, // 8 біт даних
		.parity = UART_PARITY_DISABLE, // "N" у 8N1 — без парності
		.stop_bits = UART_STOP_BITS_1, // "1" у 8N1 — один стоп-біт
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};
	ESP_ERROR_CHECK(uart_param_config(UART_PORT, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(UART_PORT, UART_TX_PIN, UART_RX_PIN,
								 UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
	ESP_ERROR_CHECK(uart_driver_install(UART_PORT, UART_BUF_SIZE, UART_BUF_SIZE,
										0, NULL, 0));
	ESP_LOGI(TAG, "UART1 ready: %d 8N1 on GPIO%d(TX)/GPIO%d(RX)", UART_BAUD,
			 UART_TX_PIN, UART_RX_PIN);
}

void gpio_init(void) {
	gpio_config_t btn_config = {.pin_bit_mask = 1ULL << BTN_PIN,
								.mode = GPIO_MODE_INPUT,
								.pull_up_en = GPIO_PULLUP_ENABLE,
								.pull_down_en = GPIO_PULLDOWN_DISABLE,
								.intr_type = GPIO_INTR_NEGEDGE};

	gpio_config(&btn_config);

	gpio_config_t led_config = {.mode = GPIO_MODE_OUTPUT,
								.pin_bit_mask = 1ULL << LED_PIN};

	gpio_config(&led_config);

	gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	xTaskCreate(button_task, "button_task", 2048, NULL, 10, NULL);
	gpio_install_isr_service(0);
	gpio_isr_handler_add(BTN_PIN, gpio_isr_handler, (void *)BTN_PIN);
}

void app_main(void) {

	uart_link_init();
	gpio_init();

	int lastState = 1;	   // Active high (pull-up default)
	int64_t startTime = 0; // Timestamp when press started (in ms)
	bool isPressedWaitingRelease = false;

	while (true) {

		if (isLedOn) {
			gpio_set_level(LED_PIN, 1);
		} else {
			gpio_set_level(LED_PIN, 0);
		}

		uart_write_bytes(UART_PORT, &isEnable, 1);
		ESP_LOGI(TAG, "Sent: %d", isEnable, isEnable);

		uint8_t rx_byte[UART_BUF_SIZE];
		int len = uart_read_bytes(UART_PORT, &rx_byte, UART_BUF_SIZE - 1,
								  pdMS_TO_TICKS(1000));
		if (len > 0) {
			ESP_LOGI(TAG, "Got: 0x%02X ('%c')", rx_byte, rx_byte);
			for (int i = 0; i < len; i++) {
				// Option A: Check for literal raw byte 0x01
				if (rx_byte[i] == 0x01) {
					isLedOn = true;
				};

				if (rx_byte[i] == 0x00) {
					isLedOn = false;
				};
			}
		}

		vTaskDelay(pdMS_TO_TICKS(10));
	}
}
