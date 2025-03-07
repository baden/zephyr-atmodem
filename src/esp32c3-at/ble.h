#pragma once

#include "../esp32c3-at.h"

int esp32_ble_start_server(struct modem_data *mdata, const char* name);
int esp32_ble_start_adv(struct modem_data *mdata);
