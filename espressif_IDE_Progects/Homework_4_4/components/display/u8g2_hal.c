#include "u8g2_hal.h"

#include "esp_log.h"
#include "esp_rom_sys.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "U8G2_HAL";
static i2c_master_dev_handle_t s_dev_handle;

void u8g2_hal_set_i2c_device(i2c_master_dev_handle_t dev_handle) {
	s_dev_handle = dev_handle;
}

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
									 void *arg_ptr) {
	switch (msg) {
	case U8X8_MSG_DELAY_MILLI:
		vTaskDelay(pdMS_TO_TICKS(arg_int));
		break;

	case U8X8_MSG_DELAY_10MICRO:
		esp_rom_delay_us(10);
		break;

	case U8X8_MSG_DELAY_100NANO:
		esp_rom_delay_us(1);
		break;

	case U8X8_MSG_GPIO_AND_DELAY_INIT:
	case U8X8_MSG_GPIO_RESET:
	default:
		// апаратний I2C, окремі GPIO для reset/clock/data не
		// використовуються break;
	}
	return 1;
}
