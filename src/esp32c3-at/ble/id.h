#pragma once
#include "../../esp32c3-at.h"

int esp32_get_mac(struct modem_data *mdata);
char *esp32_get_mac_str(struct modem_data *mdata);
