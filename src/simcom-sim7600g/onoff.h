#pragma once

#include "../simcom-sim7600g.h"

#include <zephyr/device.h>

int simcom_setup(struct modem_data *mdata, const struct modem_config *config);
int simcom_stop(struct modem_data *mdata, const struct modem_config *config);

int modem_fast_reinit(const struct device *dev);
int modem_full_reinit(const struct device *dev);
