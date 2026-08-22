#include "include/ldr_component.h"
#include "esp_adc/adc_oneshot.h"
#include <stdio.h>

adc_oneshot_unit_handle_t adc_handle;
adc_cali_handle_t cali_handle;

typedef struct {
	int buffer[SMA_WINDOW_SIZE];
	int index;
	int sum;
	bool is_full;
} sma_filter_t;

static sma_filter_t ldr_filter = {.index = 0, .sum = 0, .is_full = false};

esp_err_t adc_oneshot_read_sma(adc_oneshot_unit_handle_t handle,
							   adc_channel_t chan, int *out_raw) {
	int current_raw = 0;

	esp_err_t ret = adc_oneshot_read(handle, chan, &current_raw);
	if (ret != ESP_OK) {
		return ret;
	}

	if (ldr_filter.is_full) {

		ldr_filter.sum -= ldr_filter.buffer[ldr_filter.index];
	}

	ldr_filter.buffer[ldr_filter.index] = current_raw;
	ldr_filter.sum += current_raw;

	ldr_filter.index++;
	if (ldr_filter.index >= SMA_WINDOW_SIZE) {
		ldr_filter.index = 0;
		ldr_filter.is_full = true;
	}

	if (ldr_filter.is_full) {
		*out_raw = ldr_filter.sum / SMA_WINDOW_SIZE;
	} else {

		*out_raw = ldr_filter.sum / ldr_filter.index;
	}

	return ESP_OK;
}

void ldr_init(void) {
	adc_oneshot_unit_init_cfg_t unit_config = {
		.unit_id = ADC_UNIT,
	};
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_config, &adc_handle));

	adc_oneshot_chan_cfg_t channel_config = {
		.atten = ADC_ATTEN,
		.bitwidth = ADC_BITWIDTH,
	};
	ESP_ERROR_CHECK(
		adc_oneshot_config_channel(adc_handle, ADC_CHANNEL, &channel_config));

	adc_cali_curve_fitting_config_t cfg = {
		.unit_id = ADC_UNIT,
		.chan = ADC_CHANNEL,
		.atten = ADC_ATTEN,
		.bitwidth = ADC_BITWIDTH,
	};
	ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cfg, &cali_handle));
}

void read_ldr_value(int raw, int mv) {
	ESP_ERROR_CHECK(adc_oneshot_read_sma(adc_handle, ADC_CHANNEL, &raw));
	ESP_ERROR_CHECK(adc_cali_raw_to_voltage(cali_handle, raw, &mv));
}
