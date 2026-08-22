#include "ldr_component.h"
#include "servo.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <stdio.h>

#define SUPERLOOP_DELAY 100

// Calibration variables

static const char *TAG = "Mini Project";

void app_main(void) {
	ldr_init();
	servo_init();

	int raw;
	int mv;

	while (1) {
		read_ldr_value(&raw, &mv);

		printf("Raw ADC Value: %d\tVoltage Value: %d mV\n", raw, mv);

		set_angle(mv);

		vTaskDelay(pdMS_TO_TICKS(SUPERLOOP_DELAY));
	}
}