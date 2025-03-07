#pragma once

#include "../simcom-sim7600g.h"
#include <stdint.h>
#include <stdbool.h>

int simcom_mqtt_connect(struct modem_data *mdata, const char* host, uint16_t port, const char* client_id);
int simcom_mqtt_subscribe(struct modem_data *mdata, const char* topic, int qos);
int simcom_mqtt_publish(struct modem_data *mdata, const char* topic, const char* payload, uint8_t qos, uint16_t pub_timeout, bool retained);
int simcom_mqtt_disconnect(struct modem_data *mdata);

