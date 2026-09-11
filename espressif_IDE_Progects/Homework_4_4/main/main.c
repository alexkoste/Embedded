
#include "include/DS18B20_TEMPERATURE.h"
#include "include/bme_280.h"
#include "include/clock.h"
#include "include/display.h"
#include "include/i2c_bus.h"
#include <stdbool.h>
#include <stdio.h>
#include <unistd.h>

static const char *DAY_NAMES[] = {"???", "Sun", "Mon", "Tue",
								  "Wed", "Thu", "Fri", "Sat"};

static char ds1307_buffer[50];
static char ds1307_buffer_2[50];
static char BME280_buffer[50];
static char BME280_buffer_2[50];
static char DS18_buffer[50];

void app_main(void) {
	i2c_master_bus_handle_t bus_handle = NULL;
	spi_and_devcie_init();
	esp_err_t ret = i2c_bus_init(&bus_handle);
	if (ret != ESP_OK || bus_handle == NULL) {
		printf("CRITICAL ERROR: I2C IS NOT INITIALIZED.");
		return;
	}
	ds18B20_protocol_and_devcie_init();
	display_init(bus_handle);
	clock_init(bus_handle);

	set_current_time();
	rtc_time_t time;
	bme280_data data;
	float temp;
	while (1) {
		// Зчитуємо блок даних з 8 регістрів
		data = bme_280_read_data();
		if (ds1307_read_time(&time) == ESP_OK) {
			printf("%s %02d.%02d.20%02d | %02d:%02d:%02d",
				   DAY_NAMES[time.day_of_week], time.date, time.month,
				   time.year, time.hours, time.minutes, time.seconds);
		};
		// Виведення результату
		printf("[BME280] T: %.2f C | H: %.2f %% | P: %.2f hPa \n",
			   data.temperature, data.humidity, data.pressure);

		temp = get_temperature();

		printf("DS18: TEMP: %.2f \n", temp);

		snprintf(ds1307_buffer, sizeof(ds1307_buffer),
				 "Date: %s %02d.%02d.20%02d", DAY_NAMES[time.day_of_week],
				 time.date, time.month, time.year);
		snprintf(ds1307_buffer_2, sizeof(ds1307_buffer_2),
				 "Time: %02d:%02d:%02d", time.hours, time.minutes,
				 time.seconds);

		snprintf(BME280_buffer, sizeof(BME280_buffer), "T: %.2f C H: %.2f %%",
				 data.temperature, data.humidity);
		snprintf(BME280_buffer_2, sizeof(BME280_buffer_2), "P: %.2f hPa",
				 data.pressure);

		snprintf(DS18_buffer, sizeof(DS18_buffer), "DS18: Temp: %.2f C", temp);

		printData(ds1307_buffer, ds1307_buffer_2, BME280_buffer,
				  BME280_buffer_2, DS18_buffer);

		vTaskDelay(pdMS_TO_TICKS(500));
	}
}
