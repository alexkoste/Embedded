#ifndef MQTT_MANAGER_H
#define MQTT_MANAGER_H

// Колбек-функція для передачі отриманих з MQTT даних у головну програму
typedef void (*mqtt_data_callback_t)(const char *topic, const char *data);

void mqtt_manager_init(const char *broker_uri, const char *sub_topic,
					   mqtt_data_callback_t cb);
void mqtt_manager_publish(const char *topic, const char *payload);

#endif