#include "include/display.h"
#include "driver/i2c_master.h"
#include "u8g2.h"
#include "u8g2_hal.h"

static i2c_master_dev_handle_t oled_handle;
static u8g2_t u8g2;

uint8_t u8g2_esp32_i2c_byte_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
							   void *arg_ptr) {
	static uint8_t buffer[32];
	static uint8_t buf_idx;

	switch (msg) {
	case U8X8_MSG_BYTE_START_TRANSFER:
		buf_idx = 0;
		break;
	case U8X8_MSG_BYTE_SEND: {
		const uint8_t *data = (const uint8_t *)arg_ptr;
		for (uint8_t i = 0; i < arg_int && buf_idx < sizeof(buffer); i++)
			buffer[buf_idx++] = data[i];
		break;
	}
	case U8X8_MSG_BYTE_END_TRANSFER:
		i2c_master_transmit(oled_handle, buffer, buf_idx, 100);
		break;
	}
	return 1;
}

void display_init(i2c_master_bus_handle_t bus_handle) {

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = SSD1306_ADDR,
		.scl_speed_hz = I2C_FREQ_HZ,
	};
	ESP_ERROR_CHECK(
		i2c_master_bus_add_device(bus_handle, &dev_cfg, &oled_handle));

	u8g2_hal_set_i2c_device(oled_handle);
	u8g2_Setup_ssd1306_i2c_128x64_noname_f(
		&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);
}

void printData(const char *line_1, const char *line_2, const char *line_3,
			   const char *line_4, const char *line_5) {
	u8g2_ClearBuffer(&u8g2);
	u8g2_SetFont(&u8g2, u8g2_font_6x10_tr);
	u8g2_DrawStr(&u8g2, 0, 10, line_1);
	u8g2_DrawStr(&u8g2, 0, 20, line_2);
	u8g2_DrawStr(&u8g2, 0, 30, line_3);
	u8g2_DrawStr(&u8g2, 0, 40, line_4);
	u8g2_DrawStr(&u8g2, 0, 50, line_5);
	u8g2_SendBuffer(&u8g2);
}