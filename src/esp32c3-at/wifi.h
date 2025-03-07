#pragma once

#include "../esp32c3-at.h"

int esp32_wifi_setup(struct modem_data *mdata, const char* sid, const char* password);
int esp32_wifi_on(struct modem_data *mdata);
int esp32_wifi_off(struct modem_data *mdata);
