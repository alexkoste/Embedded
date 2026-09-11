#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_CLOCK_FREQ_HZ 100000

#define DS1307_I2C_ADDR 0x68

typedef struct {
	uint8_t seconds;
	uint8_t minutes;
	uint8_t hours;
	uint8_t day_of_week;
	uint8_t date;
	uint8_t month;
	uint8_t year;
} rtc_time_t;

void clock_init(i2c_master_bus_handle_t bus_handle);
esp_err_t ds1307_read_time(rtc_time_t *time);
void set_current_time();

#ifdef __cplusplus
}
#endif