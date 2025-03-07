#pragma once

#include "../esp32c3-at.h"

int esp32_ota_start(struct modem_data *mdata, const char* url);
