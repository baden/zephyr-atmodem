#include "at.h"
// #include <zephyr.h>
#include "ble/ble.h"
#include "server.h"
#include "../onoff.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_esp_at, CONFIG_APP_LOG_LEVEL);

int esp32_ble_init(struct modem_data *mdata, enum ble_role role)
{
    const char *send_buf = "AT+BLEINIT=0";
    switch(role) {
        case BLE_ROLE_DEINIT: send_buf = "AT+BLEINIT=0"; break;
        case BLE_ROLE_CLIENT: send_buf = "AT+BLEINIT=1"; break;
        case BLE_ROLE_SERVER: send_buf = "AT+BLEINIT=2"; break;
    }
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(1));
}

int bt_cmdq(struct modem_data *mdata)
{
    const char *send_buf = "AT+CMD?";
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(1));
}

int bt_wifi_mode(struct modem_data *mdata)
{
    const char *send_buf = "AT+CWMODE=3";
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(1));
}

int bt_rst(struct modem_data *mdata)
{
    const char *send_buf = "AT+RST";
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(1));
}


int bt_wifi_get_ip(struct modem_data *mdata)
{
    const char *send_buf = "AT+CIFSR";
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(1));
}

int bt_wifi_try_update(struct modem_data *mdata)
{
    const char *send_buf = "AT+CIUPDATE";
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(30));
}

int bt_direct_cmd_TEST(struct modem_data *mdata, const char *send_buf)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(1));
}

// AT+CIUPDATE

// AT+BLEINIT=1

// If the operator is write on a PKI bin, the <length> should be 4 bytes aligned.
//
// erase 8192 bytes from the "ble_data" partition offset 4096.
// AT+SYSFLASH=0,"ble_data",4096,8192
static int esp32_erase_bt_data_all(struct modem_data *mdata)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+SYSFLASH=0,\"ble_data\"", &mdata->sem_response, K_SECONDS(10));
}

int atmodem_erase_bt_data_all(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_erase_bt_data_all(mdata);
}

/* Handler for '>': TX Ready */
MODEM_CMD_DIRECT_DEFINE(on_cmd_tx_ready)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

	k_sem_give(&mdata->sem_tx_ready);
    // Alternative
    k_sem_give(&mdata->sem_response);
    modem_cmd_handler_set_error(data, '>');
    return len;
}

// write 10 bytes to the "ble_data" partition offset 100.
// AT+SYSFLASH=1,"ble_data",100,10

static int esp32_write_ble_data(struct modem_data *mdata, unsigned offset, unsigned length, uint8_t *buffer)
{
    int  ret;
    char send_buf[sizeof("AT+SYSFLASH=1,\"ble_data\",######,######")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+SYSFLASH=1,\"ble_data\",%d,%d", offset, length);

    /* Setup the locks correctly. */
	k_sem_take(&mdata->cmd_handler_data.sem_tx_lock, K_FOREVER);
	k_sem_reset(&mdata->sem_tx_ready);

	/* Send the Modem command. */
	ret = modem_cmd_send_nolock(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
				    NULL, 0U, send_buf, NULL, K_NO_WAIT);
	if (ret < 0) goto exit;

    struct modem_cmd handler_cmds[] = {
		MODEM_CMD_DIRECT(">", on_cmd_tx_ready),
        // MODEM_CMD("+CMGS: ", on_cmd_cmgs_mr, 1U, ""),   // Ignoring?
	};

	/* set command handlers */
	ret = modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
					    handler_cmds, ARRAY_SIZE(handler_cmds),
					    true);
	if (ret < 0) goto exit;

    /* Wait for '>' */
    #if 0
    ret = k_sem_take(&bt_mdata.sem_tx_ready, K_MSEC(5000));
    if (ret < 0) {
        /* Didn't get the data prompt - Exit. */
        LOG_DBG("Timeout waiting for tx");
        goto exit;
    }
    #else
    // TODO: We must detect "ERROR" too
    // k_sem_reset(&mdata->sem_response);
    ret = k_sem_take(&mdata->sem_response, K_MSEC(300));
	if (ret < 0) {
		LOG_DBG("No send response");
		goto exit;
	}
    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
	if (ret != '>') {
		// LOG_DBG("Failed to get >");
        goto exit;
	}

    #endif


    /* Write all data on the console */
	mdata->mctx.iface.write(&mdata->mctx.iface, buffer, length);

    /* Wait for 'OK' or 'ERROR' */
	k_sem_reset(&mdata->sem_response);
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(5));
	if (ret < 0) {
		LOG_DBG("No send response");
		goto exit;
	}

	ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
	if (ret != 0) {
		LOG_DBG("Failed to write BT data");
        goto exit;
	}

    // LOG_INF("BT data write done.");
exit:
    /* unset handler commands and ignore any errors */
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
                        NULL, 0U, false);
    k_sem_give(&mdata->cmd_handler_data.sem_tx_lock);

    if (ret < 0) {
        return ret;
    }

    return ret;
}

int esp32_write_ble_data_dev(const struct device *dev, unsigned offset, unsigned length, uint8_t *buffer)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_write_ble_data(mdata, offset, length, buffer);
}

// AT+BLEGATTSSETATTR=1,1,,1
// Notify a Client of the Value of a Characteristic Value from the Server
int esp32_ble_set_attr(struct modem_data *mdata, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    int  ret;
    char send_buf[sizeof("AT+BLEGATTSSETATTR=#,##,,####")] = {0};

    if(!bt_is_on()) return -1;

    snprintk(send_buf, sizeof(send_buf), "AT+BLEGATTSSETATTR=%d,%d,,%d", srv_index, char_index, length);

    /* Setup the locks correctly. */
    k_sem_take(&mdata->cmd_handler_data.sem_tx_lock, K_FOREVER);
    k_sem_reset(&mdata->sem_tx_ready);

    /* Send the Modem command. */
    ret = modem_cmd_send_nolock(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
                    NULL, 0U, send_buf, NULL, K_NO_WAIT);
    if (ret < 0) goto exit;

    struct modem_cmd handler_cmds[] = {
        MODEM_CMD_DIRECT(">", on_cmd_tx_ready),
        // MODEM_CMD("+CMGS: ", on_cmd_cmgs_mr, 1U, ""),   // Ignoring?
    };

    /* set command handlers */
    ret = modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
                        handler_cmds, ARRAY_SIZE(handler_cmds),
                        true);
    if (ret < 0) goto exit;

    /* Wait for '>' */
    #if 0
    ret = k_sem_take(&bt_mdata.sem_tx_ready, K_MSEC(200));
    if (ret < 0) {
        /* Didn't get the data prompt - Exit. */
        LOG_DBG("Timeout waiting for tx");
        goto exit;
    }
    #else
    // TODO: We must detect "ERROR" too
    // k_sem_reset(&mdata->sem_response);
    ret = k_sem_take(&mdata->sem_response, K_MSEC(500));
    if (ret < 0) {
        LOG_ERR("==== No send response (%d)", ret);
        goto exit;
    }
    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
    if (ret != '>') {
        // LOG_DBG("Failed to get >");
        goto exit;
    }

    #endif
    /* Write all data on the console */
    mdata->mctx.iface.write(&mdata->mctx.iface, buffer, length);

    /* Wait for 'OK' or 'ERROR' */
    k_sem_reset(&mdata->sem_response);
    ret = k_sem_take(&mdata->sem_response, K_SECONDS(1));
    if (ret < 0) {
        LOG_DBG("No send response");
        goto exit;
    }

    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
    if (ret != 0) {
        LOG_DBG("Failed to write BT data");
        goto exit;
    }

    // LOG_INF("BT data write done.");

exit:
    /* unset handler commands and ignore any errors */
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
                        NULL, 0U, false);
    k_sem_give(&mdata->cmd_handler_data.sem_tx_lock);

    if (ret < 0) {
        return ret;
    }

    return ret;
}

int esp32_ble_notify_subscribers(struct modem_data *mdata, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    for(unsigned conn_index = 0; conn_index < MAX_BT_CONNECTIONS; conn_index++) {
        if( (bt_connection[conn_index].bt_conntection_state == BT_CONNECTION_STATE_CONNECTED)
            && ((bt_connection[conn_index].notify & (1 << char_index)) != 0)
        ) {
            esp32_ble_notify(mdata, conn_index, srv_index, char_index, length, buffer);
        }
    }
    return 0;
}

// AT+BLEGATTSNTFY=0,1,6,6
// >
// Notify a Client of the Value of a Characteristic Value from the Server
int esp32_ble_notify(struct modem_data *mdata, unsigned conn_index, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    int  ret;
    char send_buf[sizeof("AT+BLEGATTSNTFY=##,##,##,##")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+BLEGATTSNTFY=%d,%d,%d,%d", conn_index, srv_index, char_index, length);

    /* Setup the locks correctly. */
	k_sem_take(&mdata->cmd_handler_data.sem_tx_lock, K_FOREVER);
	k_sem_reset(&mdata->sem_tx_ready);

	/* Send the Modem command. */
	ret = modem_cmd_send_nolock(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
				    NULL, 0U, send_buf, NULL, K_NO_WAIT);
	if (ret < 0) {
        // One fast restart?
        LOG_ERR("TODO: Fast restart?");
        goto exit;
    }

    struct modem_cmd handler_cmds[] = {
		MODEM_CMD_DIRECT(">", on_cmd_tx_ready),
        // MODEM_CMD("+CMGS: ", on_cmd_cmgs_mr, 1U, ""),   // Ignoring?
	};

	/* set command handlers */
	ret = modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
					    handler_cmds, ARRAY_SIZE(handler_cmds),
					    true);
	if (ret < 0) goto exit;

    /* Wait for '>' */
    // TODO: We must detect "ERROR" too
    // k_sem_reset(&mdata->sem_response);
    ret = k_sem_take(&mdata->sem_response, K_MSEC(300));
	if (ret < 0) {
		LOG_ERR("No send response");
		goto exit;
	}
    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
	if (ret != '>') {
		LOG_ERR("Failed to get >");
        goto exit;
	}

    /* Write all data on the console */
	mdata->mctx.iface.write(&mdata->mctx.iface, buffer, length);

    /* Wait for 'OK' or 'ERROR' */
	k_sem_reset(&mdata->sem_response);
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(1));
	if (ret < 0) {
		LOG_DBG("No send response");
		goto exit;
	}

	ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
	if (ret != 0) {
		LOG_DBG("Failed to write BT data");
        goto exit;
	}

    // LOG_INF("BT data write done.");

exit:
    /* unset handler commands and ignore any errors */
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
                        NULL, 0U, false);
    k_sem_give(&mdata->cmd_handler_data.sem_tx_lock);

    if (ret < 0) {
        // TODO: Make it more robust
        esp32_restart(mdata);

        return ret;
    }

    return ret;
}
