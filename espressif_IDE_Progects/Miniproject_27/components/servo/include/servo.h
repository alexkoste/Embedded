#include "driver/ledc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SERVO_PIN 14
#define SERVO_FREQ 50
#define SERVO_PERIOD_MS 1000 / SERVO_FREQ
#define SERVO_RESOLUTION LEDC_TIMER_12_BIT
#define SERVO_MAX_DUTY ((1 << SERVO_RESOLUTION) - 1)
#define SERVO_UNIT LEDC_TIMER_0
#define SERVO_CHANNEL LEDC_CHANNEL_0

static int SERVO_MIN_DEG = 10;
static int SERVO_MAX_DEG = 170;

static int SERVO_MIN_US = 500;
static int SERVO_MAX_US = 2500;

void servo_init();
void set_angle();

#ifdef __cplusplus
}
#endif