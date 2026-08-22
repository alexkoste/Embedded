#include "include/semaphore_component.h"
#include "driver/gpio.h"

void traffic_light_init(void) {
	gpio_config_t io_conf = {.pin_bit_mask = (1ULL << LED_RED_PIN) |
											 (1ULL << LED_YELLOW_PIN) |
											 (1ULL << LED_GREEN_PIN),
							 .mode = GPIO_MODE_OUTPUT,
							 .pull_up_en = GPIO_PULLUP_DISABLE,
							 .pull_down_en = GPIO_PULLDOWN_DISABLE,
							 .intr_type = GPIO_INTR_DISABLE};
	gpio_config(&io_conf);
	traffic_light_set(false, false, false);
}

void traffic_light_set(bool red, bool yellow, bool green) {
	gpio_set_level(LED_RED_PIN, red ? 1 : 0);
	gpio_set_level(LED_YELLOW_PIN, yellow ? 1 : 0);
	gpio_set_level(LED_GREEN_PIN, green ? 1 : 0);
}