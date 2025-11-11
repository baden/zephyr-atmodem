#include "tcp.h"
#include <zephyr/kernel.h>
#include "gprs.h"

#include<stdlib.h>  // atoi

// #include <zephyr.h>
// #include "../simcom-sim7600g.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_sim7600g, CONFIG_MODEM_LOG_LEVEL);

// // #define SERVER_IP "176.102.37.56"
// #define SERVER_IP "66.63.177.180"   // dev.delfast.bike
// // #define SERVER_PORT 31562   // DEBUG
// // #define SERVER_PORT 23725   // PROD
// // #define SERVER_PORT 30271   // UNENCRYPTED PCTEST
// #define SERVER_PORT 30272   // ENCRYPTED PCTEST
// // #define SERVER_PORT CONFIG_MODEM_TCP_DELFAST_PORT

// #define SERVER_NAME CONFIG_MODEM_TCP_DELFAST_SERVER
// #define SERVER_PORT CONFIG_MODEM_TCP_DELFAST_PORT

// TODO:
// Add check unsol message
// +CIPEVENT: NETWORK CLOSED UNEXPECTEDLY

int simcom_tcp_context_enable(struct modem_data *mdata)
{
    int ret = -1;
    // const char *send_buf = "AT+CGSOCKCONT=1,\"IP\",\"fast.t-mobile.com\"";
    ret = modem_pdp_context_enable(mdata);
    if(ret < 0) return ret;
    // AT+CGSOCKCONT=1,"IP","CMNET"
    // AT+CSOCKSETPN=1
    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+CSOCKSETPN=1", &mdata->sem_response, K_SECONDS(30));
    if(ret < 0) return ret;
    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+CIPMODE=0", &mdata->sem_response, K_SECONDS(3));
    if(ret < 0) return ret;
    ret = modem_tcp_netopen(mdata); if(ret < 0) return ret;
    modem_tcp_ipaddr(mdata);
    // k_sleep(K_SECONDS(10));
    // ret = modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, "AT+IPADDR", &mdata.sem_response, K_SECONDS(30));
    // if(ret < 0) return ret;
    // Wait: +IPADDR: 10.113.43.157
    return 0;
}

int simcom_tcp_context_disable(struct modem_data *mdata)
{
    // TODO: Wait NETCLOSE
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+NETCLOSE", &mdata->sem_response, K_SECONDS(30));
}

MODEM_CMD_DEFINE(on_cmd_conn_open)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // int link_num = ATOI(argv[0], 0, "link_num");
    modem_cmd_handler_set_error(data, 0);   //  -EIO
    k_sem_give(&mdata->sem_response);
    return 0;
}

// TODO: Register globaly:
// +IPCLOSE: <client_index>,<close_reason>
// <close_reason>:
// 0 – Closed by local, active
// 1 – Closed by remote, passive
// 2 – Closed for sending timeout

// TODO: Dirty solution
volatile bool _modem_tcp_connected = false;
int modem_tcp_on_close(struct modem_data *mdata, int client_index, int close_reason)
{
    LOG_ERR("TODO: on_cmd_ipclose (%d, %d)", client_index, close_reason);
    _modem_tcp_connected = false;
    // if(close_reason == 1) { // Closed by remote host
    //
    // }
    return 0;
}

int simcom_tcp_connect(struct modem_data *mdata, int socket_index, const char* ip, uint16_t port)
{
    int ret;
    char send_buf[sizeof("AT+CIPOPEN=##,\"TCP\",\"###.###.###.###\",######")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CIPOPEN=%d,\"TCP\",\"%s\",%d", socket_index, ip, port);

    // +CIPOPEN: <link_num>[,<type>,<serverIP>,<serverPort>,<index>]
    struct modem_cmd cmd = MODEM_CMD("+CIPOPEN:", on_cmd_conn_open, 1U, "");

    // LOG_ERR("TCP:Connect");

    ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, send_buf, &mdata->sem_response, K_SECONDS(2), MODEM_NO_UNSET_CMDS);
    if(ret < 0) {
        LOG_ERR("Error AT+CIPOPEN command");
        goto out;
    }
    k_sem_reset(&mdata->sem_response);

    /* Wait for '+HTTPACTION:' or '??' FAIL */
    // LOG_ERR("-------> 1");
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(120));

    if(ret < 0) {
        goto out;
    }

    // AT+CIPOPEN=0,\"TCP\",\"116.236.221.75\",8011
    LOG_INF("TCP: Connected");
    _modem_tcp_connected = true;

out:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);

    return ret;
}

// +NETOPEN: 0
MODEM_CMD_DEFINE(on_cmd_netopen)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // int err = ATOI(argv[0], 0, "err");
    modem_cmd_handler_set_error(data, 0);   //  -EIO
    k_sem_give(&mdata->sem_response);
    return 0;
}

int modem_tcp_netopen(struct modem_data *mdata)
{
    int ret;

    struct modem_cmd cmd = MODEM_CMD("+NETOPEN: ", on_cmd_netopen, 1U, "");

    ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, "AT+NETOPEN", &mdata->sem_response, K_SECONDS(2), MODEM_NO_UNSET_CMDS);
    if(ret < 0) {
        LOG_ERR("Error AT+NETOPEN command");
        goto out;
    }
    k_sem_reset(&mdata->sem_response);

    /* Wait for '+NETOPEN: 0' or '??' FAIL */
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(10));

    // AT+CIPOPEN=0,\"TCP\",\"116.236.221.75\",8011

out:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);

    return ret;
}

MODEM_CMD_DEFINE(on_cmd_netclose)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // int err = ATOI(argv[0], 0, "err");
    modem_cmd_handler_set_error(data, 0);   //  -EIO
    k_sem_give(&mdata->sem_response);
    return 0;
}

int modem_tcp_netclose(struct modem_data *mdata)
{
    int ret;
    LOG_ERR("Net close begin");
    _modem_tcp_connected = false;

    struct modem_cmd cmd = MODEM_CMD("+NETCLOSE: ", on_cmd_netclose, 1U, "");

    ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, "AT+NETCLOSE", &mdata->sem_response, K_SECONDS(2), MODEM_NO_UNSET_CMDS);
    if(ret < 0) {
        LOG_ERR("Error AT+NETCLOSE command");
        goto out;
    }
    k_sem_reset(&mdata->sem_response);

    /* Wait for '+NETCLOSE: 0' or '??' FAIL */
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(10));

    // AT+CIPOPEN=0,\"TCP\",\"116.236.221.75\",8011
    LOG_ERR("Net close OK");

out:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);

    return ret;
}

MODEM_CMD_DEFINE(on_cmd_conn_close)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // int link_num = ATOI(argv[0], 0, "link_num");
    modem_cmd_handler_set_error(data, 0);   //  -EIO
    k_sem_give(&mdata->sem_response);
    return 0;
}

int simcom_tcp_close(struct modem_data *mdata, int socket_index)
{
    int ret;
    char send_buf[sizeof("AT+CIPCLOSE=##")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CIPCLOSE=%d", socket_index);

    // +CIPOPEN: <link_num>[,<type>,<serverIP>,<serverPort>,<index>]
    struct modem_cmd cmd = MODEM_CMD("+CIPCLOSE:", on_cmd_conn_close, 1U, "");

    ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, send_buf, &mdata->sem_response, K_SECONDS(2), MODEM_NO_UNSET_CMDS);
    if(ret < 0) {
        LOG_ERR("Error AT+CIPCLOSE command");
        goto out;
    }
    k_sem_reset(&mdata->sem_response);

    /* Wait for '+HTTPACTION:' or '??' FAIL */
    // LOG_ERR("-------> 1");
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(10));

    // AT+CIPOPEN=0,\"TCP\",\"116.236.221.75\",8011

out:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);

    _modem_tcp_connected = false;

    return ret;
}


MODEM_CMD_DEFINE(on_cmd_ipaddr)
{
    // int link_num = ATOI(argv[0], 0, "link_num");
    // modem_cmd_handler_set_error(data, 0);   //  -EIO
    // k_sem_give(&mdata.sem_response);
    // LOG_INF("TODO: IP: %s", argv[0]);
    return 0;
}

int modem_tcp_ipaddr(struct modem_data *mdata)
{
    struct modem_cmd cmd = MODEM_CMD("+IPADDR: ", on_cmd_ipaddr, 1U, "");
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, "AT+IPADDR", &mdata->sem_response, K_SECONDS(2));
}

/* Handler: TX Ready (>) */
MODEM_CMD_DIRECT_DEFINE(on_cmd_tx_ready)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
	k_sem_give(&mdata->sem_tx_ready);
    return len;
}

/* Handler: +CIPSEND: <link_num>,<reqSendLength>,<cnfSendLength> */
MODEM_CMD_DEFINE(on_cmd_sended)
{
    // int mr = ATOI(argv[0], 0, "mr");
	return 0;
}


// AT+CIPSEND=<link_num>,<length>
int simcom_tcp_send(struct modem_data *mdata, int socket_index, const uint8_t *buffer, unsigned size)
{
    int  ret;
	char send_buf[sizeof("AT+CIPSEND=##,#####")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CIPSEND=%d,%d", socket_index, size);

    /* Setup the locks correctly. */
	k_sem_take(&mdata->cmd_handler_data.sem_tx_lock, K_FOREVER);
	k_sem_reset(&mdata->sem_tx_ready);

    /* Send the Modem command. */
	ret = modem_cmd_send_nolock(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, NULL, K_NO_WAIT);
	if (ret < 0) {
		goto exit;
	}

    struct modem_cmd handler_cmds[] = {
        MODEM_CMD_DIRECT(">", on_cmd_tx_ready),
        MODEM_CMD("+CIPSEND: ", on_cmd_sended, 3U, ","),   // Ignoring?
    };

    /* set command handlers */
	ret = modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
					    handler_cmds, ARRAY_SIZE(handler_cmds),
					    true);
	if (ret < 0) {
		goto exit;
	}

    /* Wait for '>' */
    ret = k_sem_take(&mdata->sem_tx_ready, K_MSEC(5000));
    if (ret < 0) {
        /* Didn't get the data prompt - Exit. */
        LOG_DBG("Timeout waiting for tx");
        goto exit;
    }

    /* Write all data on the console and send CTRL+Z (useless on binary data). */
	mdata->mctx.iface.write(&mdata->mctx.iface, buffer, size);
	// mctx.iface.write(&mctx.iface, &ctrlz, 1);

	/* Wait for 'SEND OK' or 'SEND FAIL' */
	k_sem_reset(&mdata->sem_response);
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(180));
	if (ret < 0) {
		LOG_DBG("No send response");
		goto exit;
	}

	ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
	if (ret != 0) {
		LOG_DBG("Failed to send");
        goto exit;
	}

exit:
    /* unset handler commands and ignore any errors */
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
                        NULL, 0U, false);
    k_sem_give(&mdata->cmd_handler_data.sem_tx_lock);

    if (ret < 0) {
        // modem_send_sms_stages = 12;
        return ret;
    }
    // modem_send_sms_stages = 13;

    return ret;
}

static int intlength(int v)
{
    if(v >= 1000) return 4;
    if(v >= 100) return 3;
    if(v >= 10) return 2;
    return 1;
}

// Receiving:
// RECV FROM:176.102.37.56:31562
// +IPD6
// 1F A5 F1 04 1D DE
// OK

// #define WRITE_DATA_CMD_MAX_LEN (sizeof("+WRITE:##,##,##,##,####,") - 1)
// The response has the form +IPD<length>\r\ndata\r\nOK\r\n

// Not best solution
/*volatile*/ uint8_t *received_buffer = NULL;
volatile size_t received_buffer_size = 0;
volatile size_t received_len;
static K_SEM_DEFINE(receive_ready, 0, 1);


// TODO: Refactor this! Move to API
#define SERVER_RECEIVE_BUFFER_SIZE 128
uint8_t unsol_buffer[SERVER_RECEIVE_BUFFER_SIZE];
void lte_server_incoming_packet(uint8_t *data, uint16_t len);

int sockread_common(struct modem_cmd_handler_data *data, int socket_data_length, uint16_t len)
{
    int ret/*, packet_size*/;
    if (!len) {
		LOG_ERR("Invalid length, aborting");
		return -EAGAIN;
	}
    if (!data->rx_buf) {
		LOG_ERR("Incorrect format! Ignoring data!");
		return -EINVAL;
	}
    if (socket_data_length <= 0) {
		LOG_ERR("Length error (%d)", socket_data_length);
		return -EAGAIN;
	}
    int skips = intlength(socket_data_length) + 2; // length value + 0x0A + 0x0D

    if (net_buf_frags_len(data->rx_buf) < (socket_data_length+skips)) {
		// LOG_DBG("Not enough data -- wait!");
		return -EAGAIN;
	}
    // LOG_ERR("socket_data_length = %d", socket_data_length);

    // Skip length value and 0x0A, 0x0D
    data->rx_buf = net_buf_skip(data->rx_buf, skips);

    if((received_buffer == NULL) || (received_buffer_size == 0)) {
        LOG_ERR("No receive buffer defined. Skip data.");
        ret = socket_data_length;
        goto exit;
    }

    ret = net_buf_linearize(received_buffer, received_buffer_size, data->rx_buf, 0, (uint16_t)socket_data_length);
    if (ret != socket_data_length) {
		LOG_ERR("Total copied data is different then received data!"
			" copied:%d vs. received:%d",
			ret, socket_data_length);
		ret = -EINVAL;
		goto exit;
	}

    data->rx_buf = net_buf_skip(data->rx_buf, ret+2);   // Skip last 0x0A, 0x0D ?
    // TODO: We must add 0x0A, 0x0D before data and 0x0A, 0x0D after data

    // LOG_ERR("=== Got it (%d)", ret);
    // LOG_HEXDUMP_ERR(received_buffer, socket_data_length, "received_buffer");
    received_len = socket_data_length;

    if(received_buffer == unsol_buffer) {
        lte_server_incoming_packet(unsol_buffer, socket_data_length);
    } else {
        k_sem_give(&receive_ready);
    }

    // TODO: Release receive buffer pointer
    received_buffer = unsol_buffer;
    received_buffer_size = SERVER_RECEIVE_BUFFER_SIZE;
    // TODO: Maybe better use two buffers and switch between them

    // TODO: We need deeply explore it
    // We must:
    // 1. not touch ret
    // 2. set ret = 0
    // 3. ret + all_skiped_data?
    ret = 0;

exit:
	/* Indication only sets length to a dummy value. */
	// packet_size = modem_socket_next_packet_size(&mdata.socket_config, sock);
	// modem_socket_packet_size_update(&mdata.socket_config, sock, -packet_size);
	return ret;
}

// MODEM_CMD_DEFINE(on_cmd_ipd)
// {
// 	return sockread_common(data, atoi(argv[0]), len);
// }

// TODO: Move receive_ready to modem_data
int simcom_tcp_recv(struct modem_data *mdata, int socket_index, uint8_t *buf, size_t max_len, size_t* readed)
{
    int ret = 0;
    received_buffer = buf;
    received_buffer_size = max_len;

    *readed = 0;
    // +IPD<n>
    // MODEM_CMD_DIRECT("+IPD", on_cmd_ipd);
    //struct modem_cmd data_cmd = MODEM_CMD("+IPD", on_cmd_ipd, 1U, ",");

    if (!buf || max_len == 0) {
		// errno = EINVAL;
		return -1;
	}
    k_sem_reset(&receive_ready);
    //(void)modem_cmd_handler_update_cmds(&mdata.cmd_handler_data, &data_cmd, 1U, false);

    ret = k_sem_take(&receive_ready, K_SECONDS(10));
    if (ret < 0) {
		LOG_ERR("Timeout waiting receive data");
        goto end;
    }

    // LOG_ERR("TODO: Gothca %d bytes", received_len);
    // LOG_HEXDUMP_ERR(buf, received_len, "<<<<");
    // if(received_len > max_len) {
    //     LOG_ERR("Input buffer is not enought for data");
    //     ret = -1;
    //     goto end;
    // }
    *readed = received_len;

    ret = 0;

end:
    //(void)modem_cmd_handler_update_cmds(&mdata.cmd_handler_data, NULL, 0U, false);
    return ret;
}

