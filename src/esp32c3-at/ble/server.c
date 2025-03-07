#include "server.h"

#include "ble/inputs.h"
#include "ble/ble.h"
#include "force/force.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_esp_at, CONFIG_APP_LOG_LEVEL);


struct bt_connection bt_connection[MAX_BT_CONNECTIONS] = {0};

bool ble_is_connected()
{
    for(unsigned conn_index = 0; conn_index < MAX_BT_CONNECTIONS; conn_index++) {
        // TODO: Use APP detected state. Just connect is not enought.
        if(bt_connection[conn_index].bt_conntection_state == BT_CONNECTION_STATE_CONNECTED) return true;
    }
    return false;
}

// TODO: Use API callbacks
void ble_regenerate_session_key();
void ble_rewrite_session_key();

void bt_server_connect(int index, const char* mac)
{
    static bool first_start = false;

    LOG_INF("TODO: BLE connected (%d, %s)", index, mac);
    if((index < 0) || (index >= MAX_BT_CONNECTIONS)) {
        LOG_ERR("Error connection index. Must be in [0.." STRINGIFY(MAX_BT_CONNECTIONS) "].");
        return;
    }
    struct bt_connection* conn = &bt_connection[index];
    conn->bt_conntection_state = BT_CONNECTION_STATE_CONNECTED;
    conn->notify = 0;   // By default no notify

// ?
    if(!first_start) {
        ble_regenerate_session_key();
        first_start = true;
    } else {
        ble_rewrite_session_key();
    }

    // Dirty hack: start new adv. Sure?
    ble_start_adv();

    // TODO: Use APP detected state. Just connect is not enought.
    cancel_autoforce();
}

// TODO: Dirty solution

#include "../force/force.h"
void esp32_ble_server_disconnect(int conn_index, const char* mac)
{
    LOG_INF("TODO: BLE disconnected (%d, %s)", conn_index, mac);

    if((conn_index < 0) || (conn_index >= MAX_BT_CONNECTIONS)) {
        LOG_ERR("Error connection index. Must be in [0.." STRINGIFY(MAX_BT_CONNECTIONS) "].");
        return;
    }
    struct bt_connection* conn = &bt_connection[conn_index];

    conn->bt_conntection_state = BT_CONNECTION_STATE_DISCONNECTED;

    // Dirty hack: start new adv. Sure?
    ble_start_adv();

    // TODO: Move to library. Here not best place for this
    if(autolock_enabled()) {
        LOG_ERR("autolock enabled");
        enum force_state cur_state = get_force_state();
        if(cur_state == FORCE_STATE_IDLE) {
            // Autolock if nobody connected
            if(!ble_is_connected()) {
                start_autoforce(AUTO_FORCE_ON_BLE);
            }
        }
    } else {
        LOG_ERR("autolock disabled");
    }
    //
}

int esp32_ble_subscribe(int conn_index, int char_index, bool subscribe)
{
    // if((conn_index < 0) || (conn_index >= MAX_BT_CONNECTIONS)) {
    //     LOG_ERR("Error connection index. Must be in [0.." STRINGIFY(MAX_BT_CONNECTIONS) "].");
    //     return -1;
    // }
    struct bt_connection* conn = &bt_connection[conn_index];

    if(subscribe) {
        conn->notify |= (1 << char_index);
    } else {
        conn->notify &= ~(1 << char_index);
    }
    return 0;
}
