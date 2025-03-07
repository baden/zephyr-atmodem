#pragma once

#include "../../esp32c3-at.h"

#include<stddef.h>

#include<zephyr/device.h>

// int bt_encode_SC(struct modem_data *mdata, char* encrypted_SC_b64, char* decrypted_SC_b64, size_t max_decrypted_SC_b64_length);
int esp32_encode_SC(const struct device *dev, char* encrypted_SC_b64, char* decrypted_SC_b64, size_t max_decrypted_SC_b64_length);
