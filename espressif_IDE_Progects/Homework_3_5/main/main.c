#include "driver/gpio.h"
#include "driver/ledc.h"
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
#include <sys/_intsup.h>
#include <unistd.h>
#include "esp_log.h"
#include "hal/ledc_types.h"
#include "soc/clk_tree_defs.h"

#define POT_ADC_CHAN ADC_CHANNEL_3
#define ADC_UNIT ADC_UNIT_1
#define SAMPLES 16

#define SERVO_PIN GPIO_NUM_7
#define LEDC_CHANNEL LEDC_CHANNEL_0

#define MIN_PULSE_WIDTH_US  500 
#define MAX_PULSE_WIDTH_US  2500

static const char *TAG = "Homework_3_5:";

static int read_avereged(adc_oneshot_unit_handle_t handle){
	long sum = 0;
	for(int i = 0; i<SAMPLES; i++){
		int raw = 0;
		adc_oneshot_read(handle, POT_ADC_CHAN, &raw);
		sum+=raw;
	}
	return(int)(sum/SAMPLES);
}

void set_servo_angle(uint32_t angle) {
    if (angle > 180) angle = 180;

    uint32_t pulse_width = MIN_PULSE_WIDTH_US + 
        ((MAX_PULSE_WIDTH_US - MIN_PULSE_WIDTH_US) * angle) / 180;

    uint32_t duty = (pulse_width * 8191) / 20000;

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);
}

void app_main(void)
{
	
			ledc_timer_config_t timer_conf = {
				.speed_mode = LEDC_LOW_SPEED_MODE,
				.timer_num = LEDC_TIMER_0,
				.duty_resolution = LEDC_TIMER_13_BIT,
				.freq_hz = 50,
				.clk_cfg = LEDC_AUTO_CLK
			};
			
			ledc_timer_config(&timer_conf);
			
			ledc_channel_config_t channel_config = {
				.sleep_mode = LEDC_LOW_SPEED_MODE,
				.channel = LEDC_CHANNEL,
				.timer_sel = LEDC_TIMER_0,
				.intr_type = LEDC_INTR_DISABLE,
				.gpio_num = SERVO_PIN,
				.duty = 0,
				.hpoint = 0
			};
			
			ledc_channel_config(&channel_config);
	
	
			adc_oneshot_unit_handle_t adc1_handle;
			adc_oneshot_unit_init_cfg_t init_config = {
				.unit_id = ADC_UNIT,
			};
			adc_oneshot_new_unit(&init_config, &adc1_handle);
			
			adc_oneshot_chan_cfg_t chan_config = {
				.bitwidth = ADC_BITWIDTH_DEFAULT,
				.atten = ADC_ATTEN_DB_12,
			};
			adc_oneshot_config_channel(adc1_handle, POT_ADC_CHAN, &chan_config);
			
			adc_cali_handle_t cali_handle = NULL;
			adc_cali_curve_fitting_config_t cali_config={
				.unit_id = ADC_UNIT,
				.chan = POT_ADC_CHAN,
				.atten = ADC_ATTEN_DB_12,
				.bitwidth = ADC_BITWIDTH_DEFAULT,
			};
			
			esp_err_t ret = adc_cali_create_scheme_curve_fitting(&cali_config, &cali_handle);
			if(ret != ESP_OK){
				ESP_LOGI(TAG, "USE another formula");
			}
			
			
			while (1) {
				int raw_pot_val = read_avereged(adc1_handle);
				
				uint32_t grad = (raw_pot_val * 180) / 4095;
				
				ESP_LOGI(TAG, "POTv = %d,	GRAD=%d", raw_pot_val, grad);
			
				set_servo_angle(grad);
	

				vTaskDelay(pdMS_TO_TICKS(20));
			}
}
