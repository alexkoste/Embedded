#pragma once

#include "driver/i2c_master.h"
#include "u8g2.h"

// Прив'язує u8g2 callback-и до вже створеного I2C device handle (новий
// driver/i2c_master.h)
void u8g2_hal_set_i2c_device(i2c_master_dev_handle_t dev_handle);

uint8_t u8g2_esp32_gpio_and_delay_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int,
									 void *arg_ptr);
