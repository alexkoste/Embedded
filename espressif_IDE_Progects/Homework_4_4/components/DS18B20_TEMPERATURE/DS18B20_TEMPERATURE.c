// clang-format off
#include "esp_log.h"
#include "soc/gpio_num.h"
#include <stdio.h>
#include "include/DS18B20_TEMPERATURE.h"
#include "ds18b20.h"
#include "onewire_bus.h"

// clang-format on

static onewire_bus_handle_t bus = NULL;
onewire_device_iter_handle_t iter = NULL;
onewire_device_t next_onewire_device;
ds18b20_device_handle_t ds18b20_handle = NULL;

void ds18B20_protocol_and_devcie_init() {
	char *TAG = "ds18B20_HELLO";
	onewire_bus_config_t temperature_bus = {
		.bus_gpio_num = ONEWIRE_BUS_GPIO,
	};
	onewire_bus_rmt_config_t rmt_config = {
		.max_rx_bytes = 10,
	};

	ESP_ERROR_CHECK(onewire_new_bus_rmt(&temperature_bus, &rmt_config, &bus));

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
}

float get_temperature() {
	float temperature = 0.0;
	ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds18b20_handle));

	if (ds18b20_get_temperature(ds18b20_handle, &temperature) == ESP_OK) {
		ESP_LOGI(TAG, "Temp: %.2f", temperature);
	} else {
		ESP_LOGE(TAG, "Reading Error");
	}
	return temperature;
}