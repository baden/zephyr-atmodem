#pragma once

#include <zephyr/kernel.h>

int simcom_ussd_request(const struct device *dev, const char *code);
