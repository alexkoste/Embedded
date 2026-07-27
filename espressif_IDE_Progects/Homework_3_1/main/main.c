#include <stdio.h>
#include <math.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "hal/adc_types.h"

#define ADC_UNIT ADC_UNIT_1
#define ADC_CHANNEL ADC_CHANNEL_6
#define ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_BITWIDTH ADC_BITWIDTH_12
#define V_REF 3.3f

static const char *TAG = "ADC_HW";

static bool init_adc_calibration( adc_unit_t unit_id, adc_channel_t channel, adc_atten_t atten,          ///< ADC attenuation
adc_bitwidth_t bitwidth, adc_cali_handle_t *out_handle ){
	bool isCalibrated = false;
	adc_cali_handle_t cali_handle = NULL;
	esp_err_t ret = ESP_FAIL;
	 
	if(!isCalibrated){
		adc_cali_curve_fitting_config_t curve_config = {
			.unit_id = unit_id,
			.chan = channel,
			.atten = atten,
			.bitwidth = bitwidth
		};
		
		ret = adc_cali_create_scheme_curve_fitting(&curve_config, &cali_handle);
		if (ret == ESP_OK) isCalibrated = true;
	}
	
	*out_handle	 = cali_handle;
	if(ret !=ESP_OK) {ESP_LOGW(TAG, "Calibrating scheme is not supported or eFuse is absent.");} 
	else{
		ESP_LOGI(TAG,"Calibrating is successful.");
	}
	return isCalibrated;
}

static void print_header(bool isCaliibrated){
	printf("\n=========================================================================\n");
	    printf("        POTENTIOMETER VOLTAGE MEASUREMENT (ESP-IDF ADC)\n");
	    printf("=========================================================================\n");
	    printf("Reference Voltage (Vref): %.2f V\n", V_REF);
	    printf("ADC Resolution:          12 bits (max 4095)\n");
	    printf("Attenuation:             12 dB (0 - 3.3V)\n");
	    printf("eFuse Calibration:       %s\n", isCaliibrated ? "ENABLED" : "DISABLED");
	    printf("-------------------------------------------------------------------------\n");
	    printf("| RAW Value | Calc Volt (V) | Calib Volt (V) | Error (%%) | Interval (ms) |\n");
	    printf("|-----------|---------------|----------------|-----------|---------------|\n");
}

void app_main(void)
{
	adc_oneshot_unit_handle_t adc_handle;
	adc_oneshot_unit_init_cfg_t init_cfg1 = {
		.unit_id = ADC_UNIT
	};
	ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_cfg1, &adc_handle));
	
	adc_oneshot_chan_cfg_t channel_cfg = {
		.atten = ADC_ATTEN,
		.bitwidth = ADC_BITWIDTH
	};
	
	ESP_ERROR_CHECK(adc_oneshot_config_channel( adc_handle, ADC_CHANNEL,  &channel_cfg));
	
	adc_cali_handle_t adc_cali_handle = NULL;
	bool is_calibrated = init_adc_calibration(ADC_UNIT, ADC_CHANNEL, ADC_ATTEN, ADC_BITWIDTH, &adc_cali_handle);
	
	
	
	int raw_val = 0;
	int voltage_mv = 0;
	print_header(is_calibrated);
	
    while (true) {
        ESP_ERROR_CHECK(adc_oneshot_read(adc_handle, ADC_CHANNEL, &raw_val));
		float calc_volts = ((float)raw_val / 4095.0f) * V_REF;
		float calib_volts = 0.0f;
		
		if (is_calibrated) {
		            ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc_cali_handle, raw_val, &voltage_mv));
		            calib_volts = voltage_mv / 1000.0f;
		        } else {
		            calib_volts = calc_volts;
		}
		
		float error_pct = 0.0f;
		if (calib_volts > 0.001f) {
			error_pct = (fabsf(calc_volts - calib_volts) / calib_volts) * 100.0f;
		}
		
		printf("|   %-7d |     %-8.3f |       %-8.3f |  %-7.2f  |     100       |\n",
			raw_val, calc_volts, calib_volts, error_pct);
		vTaskDelay(pdMS_TO_TICKS(100));
    }
}
