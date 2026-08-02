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
#include <unistd.h>
#include "esp_log.h"
#include "hal/gpio_types.h"


static const char *TAG = "Homework_3_3:";

#define LED_GPIO             GPIO_NUM_4
#define LEDC_CHANNEL_LED     LEDC_CHANNEL_0

#define MOTOR_GPIO           GPIO_NUM_5
#define LEDC_CHANNEL_MOTOR   ADC_CHANNEL_1

#define POT_ADC_CHAN ADC_CHANNEL_6
#define ADC_UNIT ADC_UNIT_1


//static adc_oneshot_unit_handle_t adc1_handle;

static void init_pwm(void){
	ledc_timer_config_t ledc_timer = {
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.timer_num = LEDC_TIMER_0,
		.duty_resolution = LEDC_TIMER_10_BIT,
		.freq_hz = 5000,
		.clk_cfg = LEDC_AUTO_CLK
	};
	
	ledc_timer_config(&ledc_timer);
	
	ledc_channel_config_t ledc_channel = {
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel = LEDC_CHANNEL_LED,
		.timer_sel = LEDC_TIMER_0,
		.intr_type = LEDC_INTR_DISABLE,
		.gpio_num = LED_GPIO,
		.duty = 0,
		.hpoint = 0
	};
	
	ledc_channel_config(&ledc_channel);
	
	ledc_channel_config_t ledc_channel_motor = {
		.speed_mode = LEDC_LOW_SPEED_MODE,
		.channel = LEDC_CHANNEL_MOTOR,
		.timer_sel = LEDC_TIMER_0,
		.intr_type = LEDC_INTR_DISABLE,
		.gpio_num = MOTOR_GPIO,
		.duty = 0,
		.hpoint = 0
	    };
		
	 ledc_channel_config(&ledc_channel_motor);
}

void app_main(void)
{
	ESP_LOGI(TAG, "Initialization");
	gpio_config_t led_conf={
			.pin_bit_mask = 1ULL<<LED_GPIO,
			.mode = GPIO_MODE_OUTPUT
		};
		
		gpio_config(&led_conf);
	    
	init_pwm();
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

	int raw_pot_val = 0;

	while (1) {
		adc_oneshot_read(adc1_handle, POT_ADC_CHAN, &raw_pot_val);
		
		uint32_t duty_cycle = raw_pot_val >> 2;
		
		ESP_LOGI(TAG, "POTv = %d,	CYCLE=%d", raw_pot_val, duty_cycle);
		
		ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_LED, duty_cycle);
		ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_MOTOR, duty_cycle);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_LED);
		ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_MOTOR);
		

		vTaskDelay(pdMS_TO_TICKS(20));
	}
}
