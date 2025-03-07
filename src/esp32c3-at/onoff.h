#pragma once

#include "../esp32c3-at.h"

int esp32_init(struct modem_data *mdata);
int esp32_setup(struct modem_data *mdata, const struct modem_config *config);
int esp32_stop(struct modem_data *mdata, const struct modem_config *config);
int esp32_restart(struct modem_data *mdata);
