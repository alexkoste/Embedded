#ifndef SENSOR_MANAGER_H
#define SENSOR_MANAGER_H

typedef struct {
	float temperature;
	float pressure; // Тиск (в гПа або мм рт. ст., наприклад hPa)
	float humidity; // Вологість
} sensor_data_t;

void sensor_init(void);
sensor_data_t sensor_read(void);

#endif