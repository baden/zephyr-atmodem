#include <zephyr/kernel.h>
#include "common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

// Commands with simple wait answer
int simcom_cmd_with_simple_wait_answer(
    struct modem_data *mdata,
    const char *send_buf,
    const struct modem_cmd handler_cmds[],
    size_t handler_cmds_len,
    k_timeout_t timeout
)
{
    /* Send the Modem command. */
    int ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
        handler_cmds, handler_cmds_len,
        send_buf, &mdata->sem_response, timeout/*K_SECONDS(2)*/, MODEM_NO_UNSET_CMDS
    );

    if (ret < 0) {
        LOG_ERR("Failed to send command [%s]. No response OK", send_buf);
        goto exit;
    }
    k_sem_reset(&mdata->sem_response);

    /* Wait for response  */
    ret = k_sem_take(&mdata->sem_response, timeout);
    if (ret < 0) {
        LOG_ERR("No got response from modem");
        goto exit;
    }
    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
    if (ret < 0) {
        LOG_ERR("Got error: %d", ret);
        goto exit;
    }

    // TODO: Maybe need callback here?

exit:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);
    return ret;
}

/* Handler for direct commands, expected data after ">" symbol */
MODEM_CMD_DIRECT_DEFINE(on_cmd_tx_ready)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
	k_sem_give(&mdata->sem_tx_ready);
    // LOG_ERR("on_cmd_tx_ready");
    return len;
}

// Command to send data to the modem after the ">" symbol
// TODO: Think about custom handlers
int simcom_cmd_with_direct_payload(
    struct modem_data *mdata,
    const char *send_buf,
    const char *data,
    size_t data_len,
    const struct modem_cmd *one_handler_cmds,
    k_timeout_t timeout
)
{
    int ret;

    struct modem_cmd handler_cmds[] = {
        MODEM_CMD_DIRECT(">", on_cmd_tx_ready),
        MODEM_CMD_DIRECT(">", on_cmd_tx_ready)  // Fake record for one_handler_cmds
    };
    if(one_handler_cmds) {
        handler_cmds[1] = *one_handler_cmds;
    }

    /* Setup the locks correctly. */
    k_sem_take(&mdata->cmd_handler_data.sem_tx_lock, K_FOREVER);
    k_sem_reset(&mdata->sem_tx_ready);

    // /* set command handlers */
    // ret = modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
    //     handler_cmds, (one_handler_cmds) ? 2 : 1,
    //     true);
    // if(ret < 0) {
    //     LOG_ERR("simcom_cmd_with_direct_payload: Failed to set command handlers");
    //     goto exit;
    // }


    /* Send the Modem command. */
    ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
        handler_cmds, (one_handler_cmds) ? 2 : 1, send_buf, NULL, K_NO_WAIT,
        MODEM_NO_TX_LOCK | MODEM_NO_UNSET_CMDS
    );

    // ret = modem_cmd_send_nolock(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
    //     NULL, 0U, send_buf, NULL, K_NO_WAIT
    // );
    if(ret < 0) {
        LOG_ERR("simcom_cmd_with_direct_payload: Failed to send command");
        goto exit;
    }


    /* Wait for '>' */
    ret = k_sem_take(&mdata->sem_tx_ready, K_MSEC(5000));
    if (ret < 0) {
        /* Didn't get the data prompt - Exit. */
        LOG_ERR("simcom_cmd_with_direct_payload: Timeout waiting for tx [%s]", send_buf);
        goto exit;
    }

    /* Write all data on the console */
    mdata->mctx.iface.write(&mdata->mctx.iface, data, data_len);


    // Is it cover both scenarios?
    // OK, then Handler
    // Handler, then OK

    /* Wait for OK or ERROR */
    k_sem_reset(&mdata->sem_response);
    ret = k_sem_take(&mdata->sem_response, timeout);
    if (ret < 0) {
        LOG_ERR("simcom_cmd_with_direct_payload: No send response");
        goto exit;
    }
    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
    if (ret < 0) {
        LOG_ERR("Got error: %d", ret);
        goto exit;
    }

    if(one_handler_cmds) {
        // Wait for second handler
        k_sem_reset(&mdata->sem_response);
        ret = k_sem_take(&mdata->sem_response, timeout);
        if (ret < 0) {
            LOG_ERR("simcom_cmd_with_direct_payload: No send response");
            goto exit;
        }
        ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
        if (ret < 0) {
            LOG_ERR("Got error: %d", ret);
            goto exit;
        }
    }

    // TODO: Maybe need callback here?

exit:
    /* unset handler commands and ignore any errors */
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
                        NULL, 0U, false);
    k_sem_give(&mdata->cmd_handler_data.sem_tx_lock);

    // if (ret < 0) {
    //     return ret;
    // }

    return ret;
}

// No operation command. Used for flushing putty buffer
int simcom_at(const struct device *dev)
{
    struct modem_data *mdata = dev->data;
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT", &mdata->sem_response, K_SECONDS(2));
}

// Free modem commad
int simcom_cmd(const struct device *dev, const char *send_buf)
{
    // char send_buf[sizeof("AT+CSSLCFG=\"cacert\",#,\"########################\"")] = {0};
    // snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"cacert\",%d,\"%s\"", ssl_ctx_index, ca_file);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(10));
}
