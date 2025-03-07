#pragma once

#include "../simcom-a7682e.h"
#include <stdint.h>
#include <stdbool.h>

// int simcom_mqtt_connect(struct modem_data *mdata, const char* host, uint16_t port, const char* client_id);
int simcom_mqtt_publish(const struct device *dev/*struct modem_data *mdata*/, const char* topic, const char* payload, uint8_t qos, uint16_t pub_timeout, bool retained);
int simcom_mqtt_disconnect(struct modem_data *mdata);


int simcom_mqtt_connect(const struct device *dev, const char* host, uint16_t port, const char* client_id);
int simcom_mqtt_subscribe(const struct device *dev/*struct modem_data *mdata*/, const char* topic, int qos);


#define CONFIG_MQTT_SSL_CTX_INDEX 1

#define MAX_MQTT_PAYLOAD_SIZE 10240
