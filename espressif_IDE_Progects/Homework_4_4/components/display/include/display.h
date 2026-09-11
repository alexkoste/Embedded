#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SSD1306_ADDR 0x3C
#define I2C_FREQ_HZ 400000

void display_init(i2c_master_bus_handle_t bus_handle);
void printData(const char *line_1, const char *line_2, const char *line_3,
			   const char *line_4, const char *line_5);

#ifdef __cplusplus
}
#endif