#pragma once

#include "../simcom-sim7600g.h"

int simcom_dns(struct modem_data *mdata, const char* domain_name, char* resolved_name);
