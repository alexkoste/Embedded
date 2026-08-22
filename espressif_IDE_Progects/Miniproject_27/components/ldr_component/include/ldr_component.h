#include <stdint.h>
#define ADC_CHANNEL ADC_CHANNEL_8 // Pin 9
#define ADC_UNIT ADC_UNIT_1
#define ADC_ATTEN ADC_ATTEN_DB_12
#define ADC_BITWIDTH ADC_BITWIDTH_12
#define SMA_WINDOW_SIZE 10

#ifdef __cplusplus
extern "C" {
#endif

static int LIGHT_MIN_MV = 50;
static int LIGHT_MAX_MV = 3000;

void ldr_init();
void read_ldr_value();
#ifdef __cplusplus
}
#endif
