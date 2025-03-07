#pragma once

#include "../simcom-a7682e.h"

int simcom_dns(struct modem_data *mdata, const char* domain_name, char* resolved_name);

int simcom_dns_config(const struct device *dev, const char* primary_dns, const char* secondary_dns);
