#include "include/clock.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include <sys/time.h>
#include <time.h>

#include <stdio.h>

static const char *TAG = "I2C_CLOCK";

static i2c_master_dev_handle_t clock_handle;

void clock_init(i2c_master_bus_handle_t bus_handle) {
	i2c_device_config_t clock_cfg = {.device_address = DS1307_I2C_ADDR,
									 .dev_addr_length = I2C_ADDR_BIT_LEN_7,
									 .scl_speed_hz = I2C_CLOCK_FREQ_HZ};
	ESP_ERROR_CHECK(
		i2c_master_bus_add_device(bus_handle, &clock_cfg, &clock_handle));
}

static uint8_t bcd2dec(uint8_t val) { return ((val >> 4) * 10) + (val & 0x0F); }

static uint8_t dec2bcd(uint8_t val) { return ((val / 10) << 4) | (val % 10); }

esp_err_t ds1307_read_time(rtc_time_t *time) {
	uint8_t reg_addr = 0x00;
	uint8_t data[7] = {0};

	esp_err_t err =
		i2c_master_transmit_receive(clock_handle, &reg_addr, 1, data, 7, -1);
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

esp_err_t ds1307_set_time(const rtc_time_t *time) {
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

void set_current_time() {
	time_t now;
	time(&now);

	// 2. Convert to local time structure (fill fields)
	struct tm timeinfo;
	localtime_r(&now, &timeinfo);
	rtc_time_t new_time = {.seconds = timeinfo.tm_sec,
						   .minutes = timeinfo.tm_min,
						   .hours = timeinfo.tm_hour,
						   .day_of_week = timeinfo.tm_wday,
						   .date = timeinfo.tm_mday,
						   .month = timeinfo.tm_mon,
						   .year = timeinfo.tm_year};

	ds1307_set_time(&new_time);
}