#pragma once
#include "soc/gpio_num.h"
#include <stdbool.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif

#define LED_RED_PIN GPIO_NUM_4
#define LED_YELLOW_PIN GPIO_NUM_5
#define LED_GREEN_PIN GPIO_NUM_6

typedef struct {
	bool red;
	bool yellow;
	bool green;
} traffic_light_state_t;

void traffic_light_init(void);
void traffic_light_set(bool red, bool yellow, bool green);

#ifdef __cplusplus
}
#endif