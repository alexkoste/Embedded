#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

#define I2C_PORT I2C_NUM_0
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9
#define I2C_FREQ_HZ 400000

static i2c_master_bus_handle_t bus_handle;

esp_err_t i2c_bus_init(i2c_master_bus_handle_t *out_bus_handle);

#ifdef __cplusplus
}
#endif