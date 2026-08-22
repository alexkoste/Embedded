#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
	STATE_RED = 0,
	STATE_YELLOW,
	STATE_RED_YELLOW,
	STATE_GREEN,
	STATE_GREEN_BLINK,
	STATE_YELLOW_BLINK // Особливий режим
} fsm_state_t;

void fsm_task(void *pvParameters);

#ifdef __cplusplus
}
#endif