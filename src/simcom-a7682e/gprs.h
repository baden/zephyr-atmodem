#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../simcom-a7682e.h"

int modem_gprs_init(struct modem_data *mdata);
int modem_gprs_done(struct modem_data *mdata);

int modem_apn(struct modem_data *mdata, char *apn, size_t len);
int modem_pdp_context_enable(struct modem_data *mdata);

