#pragma once

#include "../simcom-a7682e.h"

int modem_rssi_query_work(struct modem_data *mdata);
int simcom_rssi_query(const struct device *dev);
