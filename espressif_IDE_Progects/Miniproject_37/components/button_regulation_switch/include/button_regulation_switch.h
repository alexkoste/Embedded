#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

#define BUTTON_PIN GPIO_NUM_8

void button_init(QueueHandle_t evt_queue);

#ifdef __cplusplus
}
#endif