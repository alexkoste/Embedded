#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "include/button_regulation_switch.h"
#include "include/semaphore_component.h"
#include "include/semaphore_fsm.h"

void app_main(void) {
	// Initialization
	traffic_light_init();

	// Queue creation for btn
	QueueHandle_t button_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	button_init(button_evt_queue);

	// FSM task launch
	xTaskCreate(fsm_task, "fsm_task", 2048, (void *)button_evt_queue, 10, NULL);
}