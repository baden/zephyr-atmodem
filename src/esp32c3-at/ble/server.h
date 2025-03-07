#pragma once

#include <stdint.h>
#include <stdbool.h>

#define MAX_BT_CONNECTIONS 10

enum bt_conntection_state {
        BT_CONNECTION_STATE_DISCONNECTED = 0,
        BT_CONNECTION_STATE_CONNECTED
};

struct bt_connection {
    enum bt_conntection_state bt_conntection_state;
    uint8_t notify;
};

extern struct bt_connection bt_connection[MAX_BT_CONNECTIONS];

void bt_server_connect(int index, const char* mac);
void esp32_ble_server_disconnect(int index, const char* mac);
bool ble_is_connected();
// extern bool autolock_enabled;
