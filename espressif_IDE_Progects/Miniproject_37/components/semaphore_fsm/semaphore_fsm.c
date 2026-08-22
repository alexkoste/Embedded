#include "include/semaphore_fsm.h"
#include <stdio.h>

#include "esp_log.h"
#include "esp_log_level.h"
#include "freertos/task.h"
#include "include/semaphore_component.h"

#include <stdio.h>

static fsm_state_t current_state = STATE_RED;
static fsm_state_t previous_normal_state = STATE_RED;
static bool mode_special = false;
static const char *TAG = "DEBUG:";

void fsm_task(void *pvParameters) {
	QueueHandle_t button_queue = (QueueHandle_t)pvParameters;
	uint32_t io_num;
	TickType_t state_timer = xTaskGetTickCount();
	int blink_counter = 0;
	bool blink_flag = false;

	while (1) {
		// Перевірка натискання кнопки (без блокування виконання)
		if (xQueueReceive(button_queue, &io_num, 0) == pdTRUE) {
			// Простий дебаунс
			vTaskDelay(pdMS_TO_TICKS(50));

			mode_special = !mode_special;
			if (mode_special) {
				previous_normal_state = STATE_RED;
				current_state = STATE_YELLOW_BLINK;
			} else {
				current_state = previous_normal_state;
			}
			state_timer = xTaskGetTickCount();
			blink_counter = 0;
		}

		// Обробка станів FSM
		switch (current_state) {

		case STATE_RED:
			traffic_light_set(true, false, false);
			if ((xTaskGetTickCount() - state_timer) >=
				pdMS_TO_TICKS(5000)) { // 5 сек
				current_state = STATE_YELLOW;
				state_timer = xTaskGetTickCount();
			}
			break;

		case STATE_YELLOW:
			traffic_light_set(false, true, false);
			if ((xTaskGetTickCount() - state_timer) >=
				pdMS_TO_TICKS(2000)) { // 2 сек
				current_state = STATE_RED_YELLOW;
				state_timer = xTaskGetTickCount();
			}
			break;

		case STATE_RED_YELLOW:
			traffic_light_set(true, true, false);
			if ((xTaskGetTickCount() - state_timer) >=
				pdMS_TO_TICKS(2000)) { // 2 сек
				current_state = STATE_GREEN;
				state_timer = xTaskGetTickCount();
			}
			break;

		case STATE_GREEN:
			traffic_light_set(false, false, true);
			if ((xTaskGetTickCount() - state_timer) >=
				pdMS_TO_TICKS(5000)) { // 5 сек
				current_state = STATE_GREEN_BLINK;
				state_timer = xTaskGetTickCount();
				blink_counter = 0;
			}
			break;

		case STATE_GREEN_BLINK:
			if ((xTaskGetTickCount() - state_timer) >=
				pdMS_TO_TICKS(500)) { // Миготіння кожні 500мс
				blink_flag = !blink_flag;
				traffic_light_set(false, false, blink_flag);
				state_timer = xTaskGetTickCount();
				blink_counter++;
			}
			if (blink_counter >= 6) { // 3 повних цикли миготіння (3 сек)
				current_state = STATE_RED;
				state_timer = xTaskGetTickCount();
			}
			break;

		case STATE_YELLOW_BLINK:
			if ((xTaskGetTickCount() - state_timer) >=
				pdMS_TO_TICKS(750)) { // Миготіння жовтим
				blink_flag = !blink_flag;
				traffic_light_set(false, blink_flag, false);
				state_timer = xTaskGetTickCount();
			}
			break;
		}

		vTaskDelay(pdMS_TO_TICKS(50)); // Затримка для поступливості CPU
	}
}
