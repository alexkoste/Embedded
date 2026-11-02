#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include "include/sensor_manager.h"

void network_init(void);
void network_send_data(sensor_data_t *data);

#endif