#ifndef DISPLAY_MANAGER_H
#define DISPLAY_MANAGER_H

#include "include/sensor_manager.h"

void display_init(void);
void display_refresh(sensor_data_t *data);

#endif