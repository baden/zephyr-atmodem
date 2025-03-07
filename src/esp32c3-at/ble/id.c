#include "id.h"

#include <stdlib.h> // strtol

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_esp_at, CONFIG_MODEM_LOG_LEVEL);

/* Handler: +BLEADDR:<BLE_public_addr> */
MODEM_CMD_DEFINE(on_cmd_bleaddr)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    uint8_t *bt_mac = &mdata->modem_id.MAC[0];

    if(argv[0][0] == '"') {
        bt_mac[0] = strtol(&argv[0][1], NULL, 16);
    } else {
        bt_mac[0] = strtol(argv[0], NULL, 16);
    }
    bt_mac[1] = strtol(argv[1], NULL, 16);
    bt_mac[2] = strtol(argv[2], NULL, 16);
    bt_mac[3] = strtol(argv[3], NULL, 16);
    bt_mac[4] = strtol(argv[4], NULL, 16);
    if(argv[5][2] == '"') argv[5][2] = 0;
    bt_mac[5] = strtol(argv[5], NULL, 16);
    // modem_cmd_handler_set_error(data, 0);
	// k_sem_give(&mdata->sem_response);
    // LOG_ERR("BLE MAC %02X:%02X:%02X:%02X:%02X:%02X", bt_mac[0], bt_mac[1], bt_mac[2], bt_mac[3], bt_mac[4], bt_mac[5]);
    return 0;
}

int esp32_get_mac(struct modem_data *mdata)
{
    struct modem_cmd cmd = MODEM_CMD("+BLEADDR:", on_cmd_bleaddr, 6U, ":");
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, "AT+BLEADDR?", &mdata->sem_response, K_SECONDS(2));
}

char *esp32_get_mac_str(struct modem_data *mdata)
{
    static char mac_str[24];
    uint8_t *bt_mac = &mdata->modem_id.MAC[0];
    sprintf(mac_str, "%02X:%02X:%02X:%02X:%02X:%02X", bt_mac[0], bt_mac[1], bt_mac[2], bt_mac[3], bt_mac[4], bt_mac[5]);
    return mac_str;
}
