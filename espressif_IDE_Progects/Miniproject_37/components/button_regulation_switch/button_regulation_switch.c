#include "include/button_regulation_switch.h"
#include "driver/gpio.h"
#include <stdio.h>

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg) {
	uint32_t gpio_num = (uint32_t)arg;
	xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

void button_init(QueueHandle_t evt_queue) {
	gpio_evt_queue = evt_queue;

	gpio_config_t io_conf = {.pin_bit_mask = (1ULL << BUTTON_PIN),
							 .mode = GPIO_MODE_INPUT,
							 .pull_up_en = GPIO_PULLUP_ENABLE,
							 .pull_down_en = GPIO_PULLDOWN_DISABLE,
							 .intr_type = GPIO_INTR_NEGEDGE};
	gpio_config(&io_conf);

	gpio_install_isr_service(0);
	gpio_isr_handler_add(BUTTON_PIN, gpio_isr_handler, (void *)BUTTON_PIN);
}
