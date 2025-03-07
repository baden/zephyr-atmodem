#pragma once

#include <stdint.h>
#include <zephyr/device.h>
#include "../../esp32c3-at.h"

enum ble_role {
    BLE_ROLE_DEINIT,
    BLE_ROLE_CLIENT,
    BLE_ROLE_SERVER
};

int esp32_ble_init(struct modem_data *mdata, enum ble_role role);

int bt_cmdq(struct modem_data *mdata);
int bt_wifi_mode(struct modem_data *mdata);
int bt_rst(struct modem_data *mdata);
// int bt_wifi_connect();
int ble_wifi_setup(struct modem_data *mdata, const char* sid, const char* password);
int bt_wifi_get_ip(struct modem_data *mdata);
int bt_wifi_try_update(struct modem_data *mdata);


// int esp32_erase_bt_data_all(struct modem_data *mdata);
int atmodem_erase_bt_data_all(const struct device *dev);

// int bt_write_bt_data(struct modem_data *mdata, unsigned offset, unsigned length, uint8_t *buffer);
int esp32_write_ble_data_dev(const struct device *dev, unsigned offset, unsigned length, uint8_t *buffer);

int esp32_ble_set_attr(struct modem_data *mdata, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer);
int esp32_ble_notify_subscribers(struct modem_data *mdata, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer);
int esp32_ble_notify(struct modem_data *mdata, unsigned conn_index, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer);


// TODO: For testing only
int bt_direct_cmd_TEST(struct modem_data *mdata, const char *send_buf);
