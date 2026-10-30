#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "include/sensor_manager.h"
#include <stdbool.h>

void network_init(void);
void network_update_local_device(sensor_data_t *local_data);
int get_total_devices(void);
void get_device_info(int index, char *name, float *temp, float *pres,
					 float *hum, bool *active);

#endif