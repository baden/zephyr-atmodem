#include "handlers.h"

#include <stdlib.h>
#include <zephyr/net/buf.h>
#include "../utils.h"

#include "ble/server.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_esp_at, CONFIG_MODEM_LOG_LEVEL);

/* Handler: OK */
MODEM_CMD_DEFINE(on_cmd_ok)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    LOG_DBG("*> got OK");
	modem_cmd_handler_set_error(data, 0);
	k_sem_give(&mdata->sem_response);
	return 0;
}

/* Handler: ERROR */
MODEM_CMD_DEFINE(on_cmd_error)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // LOG_ERR("*> got ERROR");
	modem_cmd_handler_set_error(data, -EIO);
	k_sem_give(&mdata->sem_response);
	return 0;
}

static const struct modem_cmd response_cmds[] = {
	MODEM_CMD("OK", on_cmd_ok, 0U, ""),
	MODEM_CMD("ERROR", on_cmd_error, 0U, ""),
	// MODEM_CMD("+CME ERROR: ", on_cmd_exterror, 1U, ""),
};

// static volatile bool _wait_ready = false;
/* Handler: ready */
MODEM_CMD_DEFINE(on_cmd_unsol_ready)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
#if 0
    if(_wait_ready) {
        LOG_ERR("*> goted ready");
        modem_cmd_handler_set_error(data, 0);
        k_sem_give(&mdata->sem_response);
    } else {
        LOG_ERR("*> unexpected ready (sudden restart?)");
        // Unexpected restarting
        bt_sudn_restart();
    }
#endif
    k_work_submit_to_queue(&mdata->workq, &mdata->init_work);
	return 0;
}

// +BLECONN:1,"a8:66:7f:29:00:49"
// MODEM_CMD_DEFINE(on_cmd_unsol_connect)
// {
// }
/* Func: on_cmd_sockread_common
 * Desc: Function to successfully read data from the modem on a given socket.
 */
// static int on_cmd_sockread_common(
// 				  struct modem_cmd_handler_data *data,
// 				  uint16_t len)
// {
//     return 0;
// }



// +WRITE:<conn_index>,<srv_index>,<char_index>,[<desc_index>],<len>,<value>
// Example:
// +WRITE:1,1,3,,9,IP0123456
/* Handler: Read data */

void bt_cmd_write(int conn_index, int srv_index, int char_index, int desc_index, int payload_lengh, uint8_t* p);

#define WRITE_DATA_CMD_MIN_LEN (sizeof("+WRITE:,,,,,") - 1)
#define WRITE_DATA_CMD_MAX_LEN (sizeof("+WRITE:##,##,##,##,####,") - 1)
MODEM_CMD_DIRECT_DEFINE(on_cmd_sock_write)
{
    // char cmd_buf[WRITE_DATA_CMD_MAX_LEN + 1];
    // dev = CONTAINER_OF(data, struct esp_data, cmd_handler_data);
    // LOG_ERR("TODO: Got +WRITE:...%d bytes", len);
    size_t frags_len = net_buf_frags_len(data->rx_buf);
    // LOG_ERR("frags_len = %d", frags_len);

    if (frags_len < WRITE_DATA_CMD_MIN_LEN) {
		return -EAGAIN;
	}
    LOG_DBG("TODO: Got +WRITE:...%d bytes", frags_len);

    // if (frags_len < WRITE_DATA_CMD_MIN_LEN) {
	// 	return -EAGAIN;
	// }

    // size_t match_len = net_buf_linearize(cmd_buf, WRITE_DATA_CMD_MAX_LEN, data->rx_buf, 0, WRITE_DATA_CMD_MIN_LEN);
    // cmd_buf[match_len] = 0;
    //
    // LOG_ERR("match_len = %d", match_len);

    // Count ','s must be 5
    char* p = data->rx_buf->data;
    unsigned quotes = 0;
    // for(unsigned i=0; i<match_len; i++) {
    for(unsigned i = len; i < frags_len; i++) {
        // if(cmd_buf[i] == ',') quotes++;
        if(p[i] == ',') quotes++;
    }
    if(quotes < 5) {
        if(frags_len /*match_len*/ >= WRITE_DATA_CMD_MAX_LEN) {
            LOG_ERR("Not enought ',' and WRITE_DATA_CMD_MAX_LEN (Invalid cmd?)");
            return len;
        }
        return -EAGAIN;
    }

    // LOG_ERR("Good cmd?");
    //LOG_HEXDUMP_ERR(p, frags_len, "p");
    // +WRITE:<conn_index>,<srv_index>,<char_index>,[<desc_index>],<len>,<value>
    // Example:
    // +WRITE:1,1,3,,9,IP0123456
    size_t parsed_bytes = 0;
    int conn_index = 0, srv_index = 0, char_index = 0, desc_index = 0, payload_lengh = 0;
    int res;
    p += len; // Skip +WRITE:

    FIND_VAL(res, p, frags_len, parsed_bytes, conn_index)
    FIND_VAL(res, p, frags_len, parsed_bytes, srv_index)
    FIND_VAL(res, p, frags_len, parsed_bytes, char_index)
    FIND_VAL(res, p, frags_len, parsed_bytes, desc_index)
    FIND_VAL(res, p, frags_len, parsed_bytes, payload_lengh)

    // res = find_val(p, frags_len, &parsed_bytes, &conn_index);
    // LOG_ERR("res = %d  parsed_bytes = %d", res, parsed_bytes);
    // if(res == -2) {
    //     printk("*");
    //     return -EAGAIN;
    // } else if(res == -1) {
    //     LOG_ERR("Error parsing conn_index");
    //     return len; // Skip that data?
    // }
    // p += parsed_bytes;

    // LOG_DBG("conn_index=%d srv_index=%d char_index=%d,desc_index=%d payload_lengh=%d", conn_index, srv_index, char_index, desc_index, payload_lengh);

    // frags_len

    size_t data_offset = (uint32_t)p - (uint32_t)data->rx_buf->data; // Is it correct math operation?
    // LOG_DBG("offset = %d", data_offset);

    /*
        FIXME: Inefficient way of waiting for data
        Better way is use
            net_buf_pull_u8
            net_buf_frag_del
            and
            net_buf_linearize
    */
	if (data_offset + payload_lengh > frags_len) {
        // LOG_ERR("Wait a bit more.");
		return -EAGAIN;
	}

    // TODO: Our payload data:
    // LOG_HEXDUMP_ERR(p, payload_lengh, "payload");

    // TODO: Use callback over API to notify about new data
    bt_cmd_write(conn_index, srv_index, char_index, desc_index, payload_lengh, p);

    return data_offset + payload_lengh; // Skip that data?
#if 0
    if (!len) {
		// LOG_ERR("Invalid length, Aborting!");
		return -EAGAIN;
	}
    /* Make sure we still have buf data */
	if (!data->rx_buf) {
		LOG_ERR("Incorrect format! Ignoring data!");
		return -EINVAL;
	}


    LOG_ERR("data->rx_buf->len = %d", data->rx_buf->len);

    const char* p = data->rx_buf->data;
    // int conn_index = find_val(p, size_t data_length, size_t *parsed_bytes, int* v)

    int socket_data_length = find_len(data->rx_buf->data);
    LOG_HEXDUMP_ERR(data->rx_buf->data, 16, "data->rx_buf->data");

    LOG_ERR("Find length: %d", socket_data_length);

    return 0;
#endif
	// return on_cmd_sockread_common(data, len);
}


// +BLECONN:0,"65:50:73:b7:a5:f9"
MODEM_CMD_DEFINE(on_cmd_ble_connect)
{
    int index = ATOI(argv[0], 0, "Connection index");
    char *mac;
    mac = str_unquote(argv[1]);
    bt_server_connect(index, mac);
    return 0;
}

// +BLEDISCONN:0,"65:50:73:b7:a5:f9"
MODEM_CMD_DEFINE(on_cmd_ble_disconnect)
{
    int index = ATOI(argv[0], 0, "Connection index");
    char *mac;
    mac = str_unquote(argv[1]);
    esp32_ble_server_disconnect(index, mac);
    return 0;
}

// +WRITE:0,1,1,,9,IP0123456
static const struct modem_cmd unsol_cmds[] = {
	// MODEM_CMD("+QIURC: \"recv\",",	   on_cmd_unsol_recv,  1U, ""),
	// MODEM_CMD("+QIURC: \"closed\",",   on_cmd_unsol_close, 1U, ""),
	MODEM_CMD("ready", on_cmd_unsol_ready, 0U, ""),
    MODEM_CMD_DIRECT("+WRITE:", on_cmd_sock_write),    // TODO: Maybe MODEM_CMD_DIRECT ?
    MODEM_CMD("+BLECONN:", on_cmd_ble_connect, 2U, ","),
    MODEM_CMD("+BLEDISCONN:", on_cmd_ble_disconnect, 2U, ","),
};


NET_BUF_POOL_DEFINE(recv_pool, MDM_RECV_MAX_BUF, MDM_RECV_BUF_SIZE, 0, NULL);

int esp32at_init_handlers(struct modem_data *mdata)
{
    /* cmd handler */
    mdata->cmd_handler_data.cmds[CMD_RESP]	    = response_cmds;
	mdata->cmd_handler_data.cmds_len[CMD_RESP]  = ARRAY_SIZE(response_cmds);
    mdata->cmd_handler_data.cmds[CMD_UNSOL]	    = unsol_cmds;
	mdata->cmd_handler_data.cmds_len[CMD_UNSOL] = ARRAY_SIZE(unsol_cmds);
    mdata->cmd_handler_data.match_buf	        = &mdata->cmd_match_buf[0];
	mdata->cmd_handler_data.match_buf_len	    = sizeof(mdata->cmd_match_buf);
	mdata->cmd_handler_data.buf_pool		    = &recv_pool;
	mdata->cmd_handler_data.alloc_timeout	    = K_NO_WAIT;
	mdata->cmd_handler_data.eol		            = "\r\n";
	return modem_cmd_handler_init(&mdata->mctx.cmd_handler, &mdata->cmd_handler_data);
}
