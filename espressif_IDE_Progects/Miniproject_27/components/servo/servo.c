#include "include/servo.h"
#include "driver/ledc.h"
#include "ldr_component.h"

void servo_init() {
	ledc_timer_config_t timer_config = {.speed_mode = LEDC_LOW_SPEED_MODE,
										.timer_num = SERVO_UNIT,
										.duty_resolution = SERVO_RESOLUTION,
										.freq_hz = SERVO_FREQ,
										.clk_cfg = LEDC_AUTO_CLK};
	ESP_ERROR_CHECK(ledc_timer_config(&timer_config));

	ledc_channel_config_t channel_config = {.speed_mode = LEDC_LOW_SPEED_MODE,
											.channel = SERVO_CHANNEL,
											.timer_sel = SERVO_UNIT,
											.intr_type = LEDC_INTR_DISABLE,
											.gpio_num = SERVO_PIN,
											.duty = 0};
	ESP_ERROR_CHECK(ledc_channel_config(&channel_config));
}

static int val_clamp(int value, int val_min, int val_max) {
	if (value < val_min)
		return val_min;
	if (value > val_max)
		return val_max;
	return value;
}

static int mad_map(int value, int in_min, int in_max, int out_min,
				   int out_max) {
	if (in_min == in_max)
		return out_max;

	value = val_clamp(value, in_min, in_max);

	return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

void set_angle(int mv) {
	mv = val_clamp(mv, LIGHT_MIN_MV, LIGHT_MAX_MV);

	int val_deg =
		mad_map(mv, LIGHT_MIN_MV, LIGHT_MAX_MV, SERVO_MIN_DEG, SERVO_MAX_DEG);
	printf("val_deg = %d\n", val_deg);
	int val_us = mad_map(val_deg, SERVO_MIN_DEG, SERVO_MAX_DEG, SERVO_MIN_US,
						 SERVO_MAX_US);
	printf("val_us = %d\n", val_us);
	printf("SERVO_MAX_DUTY = %d\n", SERVO_MAX_DUTY);

	// uint32_t duty = (uint32_t)(SERVO_MAX_DUTY / SERVO_PERIOD_MS) * val_us /
	// 1000; // 205 * val_ms
	uint32_t duty = (uint32_t)(4095 / 20) * val_us / 1000; // 205 * val_ms
	printf("Duty = %u\n", (unsigned int)duty);

	ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty));
	ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL));
}