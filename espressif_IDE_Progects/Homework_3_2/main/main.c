#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_err.h"
#include "esp_private/adc_private.h"
#include "esp_timer.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "hal/adc_types.h"
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include "esp_log.h"
#include "hal/gpio_types.h"

#define ADC_UNIT ADC_UNIT_1
#define ADC_CHANNEL ADC_CHANNEL_6
#define LED_PIN GPIO_NUM_5

#define SAMPLES 16
#define SAMPLEXPERIOD_US (50*1000)
#define THRASHOLD_ON_RAW 2700
#define THRASHOLD_OF_RAW 3900

#define MAX_NUMBER_BITS 4095
#define MAX_VOLTS_REF 3300

static const char *TAG = "HW_3_3";

static int read_avereged(adc_oneshot_unit_handle_t handle){
	long sum = 0;
	for(int i = 0; i<SAMPLES; i++){
		int raw = 0;
		adc_oneshot_read(handle, ADC_CHANNEL, &raw);
		sum+=raw;
	}
	return(int)(sum/SAMPLES);
}

void app_main(void)
{
	gpio_config_t led_conf={
			.pin_bit_mask = 1ULL<<LED_PIN,
			.mode = GPIO_MODE_OUTPUT
		};
		
		gpio_config(&led_conf);
		
		adc_oneshot_unit_handle_t adc1_handle;
		adc_oneshot_unit_init_cfg_t init_config = {
			.unit_id = ADC_UNIT,
		};
		adc_oneshot_new_unit(&init_config, &adc1_handle);
		
		adc_oneshot_chan_cfg_t chan_config = {
			.bitwidth = ADC_BITWIDTH_DEFAULT,
			.atten = ADC_ATTEN_DB_12,
		};
		adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL, &chan_config);
		
		adc_cali_handle_t cali_handle = NULL;
		adc_cali_curve_fitting_config_t cali_config={
			.unit_id = ADC_UNIT,
			.chan = ADC_CHANNEL,
			.atten = ADC_ATTEN_DB_12,
			.bitwidth = ADC_BITWIDTH_DEFAULT,
		};
		
		esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
		if(ret != ESP_OK){
			ESP_LOGI(TAG, "USE another formula");
		}
		
		bool led_on = false;
		int64_t last_sample=0;
		
		
		while(true){
			int64_t now = esp_timer_get_time();
			if(now-last_sample>=SAMPLEXPERIOD_US){
				last_sample = esp_timer_get_time();
				int level = read_avereged(adc1_handle);
				
				int voltage_mv_calibrate = 0;
				adc_cali_raw_to_voltage(cali_handle, level, &voltage_mv_calibrate);
				
				if(level<THRASHOLD_ON_RAW){
					led_on = true;
				}else if (level<THRASHOLD_OF_RAW){
					led_on = false;
				}
				
				gpio_set_level(LED_PIN, led_on);
				
				ESP_LOGI(TAG, "level=%4d (%dmv)\tled=%s", level, voltage_mv_calibrate, led_on?"ON":"OFF");
			}
			vTaskDelay(pdTICKS_TO_MS(10));
}
}
