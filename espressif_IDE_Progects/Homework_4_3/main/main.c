#include "driver/i2c_master.h"
#include "ds18b20.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "hal/i2c_types.h"
#include "onewire_bus.h"
#include "u8g2.h"
#include "u8g2_hal.h"
#include <stdio.h>
#include <string.h>

#define I2C_PORT I2C_NUM_0
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define I2C_FREQ_HZ 400000
#define SSD1306_ADDR 0x3C

#define I2C_CLOCK_FREQ_HZ 100000

#define DS1307_I2C_ADDR 0x68

#define ONEWIRE_BUS_GPIO GPIO_NUM_4
#define SENSOR_MAX_NUM 1

static const char *TAG = "SSD1306_HELLO";
static i2c_master_bus_handle_t bus_handle;
static i2c_master_dev_handle_t oled_handle;
static i2c_master_dev_handle_t clock_handle;
static onewire_bus_handle_t bus = NULL;
static u8g2_t u8g2;
static char buffer[50];
static char time_buffer[50];
static char temperature_buffer[50];
static const char *DAY_NAMES[] = {"???", "Sun", "Mon", "Tue",
								  "Wed", "Thu", "Fri", "Sat"};

void i2c_bus_init(void) {
	i2c_master_bus_config_t bus_config = {
		.clk_source = I2C_CLK_SRC_DEFAULT,
		.i2c_port = I2C_PORT,
		.scl_io_num = I2C_SCL_PIN,
		.sda_io_num = I2C_SDA_PIN,
		.glitch_ignore_cnt = 7,
		.flags.enable_internal_pullup = false,
	};
	ESP_ERROR_CHECK(i2c_new_master_bus(&bus_config, &bus_handle));

	i2c_device_config_t clock_cfg = {.device_address = DS1307_I2C_ADDR,
									 .dev_addr_length = I2C_ADDR_BIT_LEN_7,
									 .scl_speed_hz = I2C_CLOCK_FREQ_HZ};

	ESP_ERROR_CHECK(
		i2c_master_bus_add_device(bus_handle, &clock_cfg, &clock_handle));

	i2c_device_config_t dev_cfg = {
		.dev_addr_length = I2C_ADDR_BIT_LEN_7,
		.device_address = SSD1306_ADDR,
		.scl_speed_hz = I2C_FREQ_HZ,
	};
	ESP_ERROR_CHECK(
		i2c_master_bus_add_device(bus_handle, &dev_cfg, &oled_handle));

	onewire_bus_config_t temperature_bus = {
		.bus_gpio_num = ONEWIRE_BUS_GPIO,
	};
	onewire_bus_rmt_config_t rmt_config = {
		.max_rx_bytes = 10,
	};

	ESP_ERROR_CHECK(onewire_new_bus_rmt(&temperature_bus, &rmt_config, &bus));
}

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

// CLOCK
typedef struct {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t day_of_week;
	uint8_t date;
	uint8_t month;
	uint8_t year;
} rtc_time_t;

static uint8_t bcd2dec(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

static uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }

esp_err_t ds1307_read_time(i2c_master_dev_handle_t dev_handle,
						   rtc_time_t *time) {
	uint8_t reg_addr = 0x00;
	uint8_t data[7] = {0};

	esp_err_t err =
		i2c_master_transmit_receive(dev_handle, &reg_addr, 1, data, 7, -1);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "I2C read failed: %s", esp_err_to_name(err));
		return err;
	}

	time->seconds = bcd2dec(data[0] & 0x7F);
	time->minutes = bcd2dec(data[1] & 0x7F);
	time->hours = bcd2dec(data[2] & 0x3F);
	time->day_of_week = bcd2dec(data[3] & 0x07);
	time->date = bcd2dec(data[4] & 0x3F);
	time->month = bcd2dec(data[5] & 0x1F);
	time->year = bcd2dec(data[6]);

	return ESP_OK;
}

static void bufferInit() { memset(buffer, 0, sizeof(buffer)); }

static esp_err_t ds1307_set_time(const rtc_time_t *time) {
	if (clock_handle == NULL)
		return ESP_ERR_INVALID_STATE;

	uint8_t write_buf[8];
	write_buf[0] = 0x00;
	write_buf[1] = dec2bcd(time->seconds) & 0x7F;
	write_buf[2] = dec2bcd(time->minutes) & 0x7F;
	write_buf[3] = dec2bcd(time->hours) & 0x3F;
	write_buf[4] = dec2bcd(time->day_of_week) & 0x07;
	write_buf[5] = dec2bcd(time->date) & 0x3F;
	write_buf[6] = dec2bcd(time->month) & 0x1F;
	write_buf[7] = dec2bcd(time->year);

	esp_err_t err =
		i2c_master_transmit(clock_handle, write_buf, sizeof(write_buf), -1);
	if (err == ESP_OK) {
		ESP_LOGI(TAG, "Successfully set RTC time.");
	} else {
		ESP_LOGE(TAG, "Failed to set RTC time: %s", esp_err_to_name(err));
	}
	return err;
}

// CLOCK

void app_main(void) {
	bufferInit();
	i2c_bus_init();
	u8g2_hal_set_i2c_device(oled_handle);
	u8g2_Setup_ssd1306_i2c_128x64_noname_f(
		&u8g2, U8G2_R0, u8g2_esp32_i2c_byte_cb, u8g2_esp32_gpio_and_delay_cb);
	u8g2_InitDisplay(&u8g2);
	u8g2_SetPowerSave(&u8g2, 0);

	rtc_time_t new_time = {.seconds = 0,
						   .minutes = 20,
						   .hours = 14,
						   .day_of_week = 7,
						   .date = 5,
						   .month = 9,
						   .year = 26};

	ds1307_set_time(&new_time);

	onewire_device_iter_handle_t iter = NULL;
	onewire_device_t next_onewire_device;
	ds18b20_device_handle_t ds18b20_handle = NULL;

	ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
	if (onewire_device_iter_get_next(iter, &next_onewire_device) == ESP_OK) {
		ds18b20_config_t ds_cfg = {};
		ESP_ERROR_CHECK(ds18b20_new_device_from_enumeration(
			&next_onewire_device, &ds_cfg, &ds18b20_handle));
		ESP_LOGI(TAG, "Found DS18B20 (Address: %016llX)",
				 next_onewire_device.address);
	} else {
		ESP_LOGE(TAG, "Device DS18B20 is not found");
	}
	ESP_ERROR_CHECK(onewire_del_device_iter(iter));

	if (ds18b20_handle == NULL) {
		ESP_LOGE(TAG, "Error on initialization"
					  "resistor 4.7k.");
		return;
	}

	float temperature = 0.0;
	rtc_time_t time;

	while (1) {
		ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds18b20_handle));

		if (ds18b20_get_temperature(ds18b20_handle, &temperature) == ESP_OK) {
			ESP_LOGI(TAG, "Temp: %.2f", temperature);
		} else {
			ESP_LOGE(TAG, "Reading Error");
		}

		snprintf(temperature_buffer, sizeof(temperature_buffer),
				 "Temp: %.2f Celsius", temperature);

		if (ds1307_read_time(clock_handle, &time) == ESP_OK) {
			ESP_LOGI(TAG, "%s %02d.%02d.20%02d | %02d:%02d:%02d",
					 DAY_NAMES[time.day_of_week], time.date, time.month,
					 time.year, time.hours, time.minutes, time.seconds);
		};
		snprintf(buffer, sizeof(buffer), "Date: %s %02d.%02d.20%02d",
				 DAY_NAMES[time.day_of_week], time.date, time.month, time.year);
		snprintf(time_buffer, sizeof(time_buffer), "Time: %02d:%02d:%02d",
				 time.hours, time.minutes, time.seconds);

		u8g2_ClearBuffer(&u8g2);
		u8g2_SetFont(&u8g2, u8g2_font_ncenB08_tr);
		u8g2_DrawStr(&u8g2, 0, 10, buffer);
		u8g2_DrawStr(&u8g2, 0, 30, time_buffer);
		u8g2_DrawStr(&u8g2, 0, 50, temperature_buffer);
		u8g2_SetFont(&u8g2, u8g2_font_unifont_t_symbols);
		u8g2_DrawUTF8(&u8g2, 50, 65, "♥");
		u8g2_SendBuffer(&u8g2);

		vTaskDelay(pdMS_TO_TICKS(1000));
	}
}