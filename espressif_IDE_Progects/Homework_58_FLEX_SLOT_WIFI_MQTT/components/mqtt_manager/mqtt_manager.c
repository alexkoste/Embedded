#include "include/mqtt_manager.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>

#define TAG "MQTT_MANAGER"

static esp_mqtt_client_handle_t client = NULL;
static char s_sub_topic[64] = {0};
static mqtt_data_callback_t s_user_callback = NULL;

static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
							   int32_t event_id, void *event_data) {
	esp_mqtt_event_handle_t event = event_data;

	switch ((esp_mqtt_event_id_t)event_id) {
	case MQTT_EVENT_CONNECTED:
		ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
		if (strlen(s_sub_topic) > 0) {
			esp_mqtt_client_subscribe(client, s_sub_topic, 0);
			ESP_LOGI(TAG, "Subscribed to %s", s_sub_topic);
		}
		break;

	case MQTT_EVENT_DATA: {
		char topic_buf[128] = {0};
		char data_buf[64] = {0};

		int t_len = event->topic_len < sizeof(topic_buf) - 1
						? event->topic_len
						: sizeof(topic_buf) - 1;
		int d_len = event->data_len < sizeof(data_buf) - 1
						? event->data_len
						: sizeof(data_buf) - 1;

		memcpy(topic_buf, event->topic, t_len);
		memcpy(data_buf, event->data, d_len);

		if (s_user_callback) {
			s_user_callback(topic_buf, data_buf);
		}
		break;
	}
	default:
		break;
	}
}

void mqtt_manager_init(const char *broker_uri, const char *sub_topic,
					   mqtt_data_callback_t cb) {
	strncpy(s_sub_topic, sub_topic, sizeof(s_sub_topic) - 1);
	s_user_callback = cb;

	esp_mqtt_client_config_t mqtt_cfg = {
		.broker.address.uri = broker_uri,
	};

	client = esp_mqtt_client_init(&mqtt_cfg);
	esp_mqtt_client_register_event(client, ESP_EVENT_ANY_ID, mqtt_event_handler,
								   NULL);
	esp_mqtt_client_start(client);
}

void mqtt_manager_publish(const char *topic, const char *payload) {
	if (client) {
		esp_mqtt_client_publish(client, topic, payload, 0, 1, 0);
	}
}