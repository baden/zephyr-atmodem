#include "http.h"
#include "id.h"
#include "gprs.h"
#include <zephyr/kernel.h>
#include "../utils.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

int modem_http_init(struct modem_data *mdata)
{
    int ret = -1;
    unsigned retryes = 0;
    const char *send_buf = "AT+HTTPINIT";

    while(retryes < 3) {
        ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(30));
        if(ret >= 0) return ret;
        retryes++;
        modem_http_done(mdata);
    }

    return ret;
}

int simcom_http_init(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return modem_http_init(mdata);
}

int modem_http_done(struct modem_data *mdata)
{
    const char *send_buf = "AT+HTTPTERM";
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(3));
}

int simcom_http_done(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return modem_http_done(mdata);
}

int simcom_http_set_cid(const struct device *dev, int cid)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    char send_buf[sizeof("AT+HTTPPARA=\"CID\",#")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+HTTPPARA=\"CID\",%d", cid);

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}


// AT+HTTPPARA="URL","<url>"
// AT+HTTPPARA="CONNECTTO",<conn_timeout>
// AT+HTTPPARA="RECVTO",<recv_timeout>
// AT+HTTPPARA="CONTENT","<content_type>"
// AT+HTTPPARA="ACCEPT","<accept-type>"
// AT+HTTPPARA="UA","<user_agent>"
// AT+HTTPPARA="SSLCFG","<sslcfg_id>"
// AT+HTTPPARA="USERDATA","<user_data>"
// AT+HTTPPARA="BREAK",<break>
// AT+HTTPPARA="BREAKEND",<breakend>

static int modem_http_set_content_type(struct modem_data *mdata, const char* content_type)
{
    char send_buf[sizeof("AT+HTTPPARA=\"CONTENT\",\"#########################################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+HTTPPARA=\"CONTENT\",\"%s\"", content_type);

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

static int modem_http_set_accept_type(struct modem_data *mdata, const char* content_type)
{
    char send_buf[sizeof("AT+HTTPPARA=\"ACCEPT\",\"#########################################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+HTTPPARA=\"ACCEPT\",\"%s\"", content_type);

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

static int modem_http_set_user_data(struct modem_data *mdata, const char* value)
{
    // struct modem_data *mdata = dev->data;
    char send_buf[sizeof("AT+HTTPPARA=\"USERDATA\",\"#########################################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+HTTPPARA=\"USERDATA\",\"%s\"", value);

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

int modem_http_set_ssl_cfg(struct modem_data *mdata, int id)
{
    char send_buf[sizeof("AT+HTTPPARA=\"SSLCFG\",#####")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+HTTPPARA=\"SSLCFG\",%d", id);

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}


int simcom_http_set_ssl_cfg(const struct device *dev, int id)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return modem_http_set_ssl_cfg(mdata, id);
}


// TODO: Make modem_http_set_url printf like.
// You can use iface->write(iface, buf, buf_len) for sending stream
static int modem_http_set_url(struct modem_data *mdata, const char* url)
{
    char send_buf[256] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+HTTPPARA=\"URL\",\"%s\"", url);

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

// TODO: Move to modem_data or somewhere else
static struct http_last_action_result http_last_action_result;

// static K_SEM_DEFINE(http_action, 0, 1);

// Wait +HTTPACTION: <method>,<status_code>,<datalen>
MODEM_CMD_DEFINE(on_cmd_http_action)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

    http_last_action_result.success = true;
    http_last_action_result.method = ATOI(argv[0], 0, "method");
    http_last_action_result.status_code = ATOI(argv[1], 0, "status_code");
    http_last_action_result.datalen = ATOI(argv[2], 0, "datalen");
    LOG_DBG("*> Got +HTTPACTION=%d,%d,%d"
            , http_last_action_result.method
            , http_last_action_result.status_code
            , http_last_action_result.datalen );
    modem_cmd_handler_set_error(data, 0);   //  -EIO
    k_sem_give(&mdata->sem_http_action_response);
    // k_sem_give(&http_action);
	// k_sem_give(&dev->sem_response);
	return 0;
}

static int modem_http_doit(struct modem_data *mdata, enum http_method method)
{
    int ret;
    char send_buf[sizeof("AT+HTTPACTION=##")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+HTTPACTION=%d", (method == HTTP_GET) ? 0 : 1);

    struct modem_cmd cmd = MODEM_CMD("+HTTPACTION:", on_cmd_http_action, 3U, ",");

    http_last_action_result.success = false;
    // k_sem_reset(&http_action);
    ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, send_buf, &mdata->sem_response, K_SECONDS(2), MODEM_NO_UNSET_CMDS);
    if(ret < 0) {
        LOG_ERR("Error HTTPACTION= command");
        goto out;
    }
    k_sem_reset(&mdata->sem_http_action_response);

    /* Wait for '+HTTPACTION:' or '??' FAIL */
    // LOG_ERR("-------> 1");
	ret = k_sem_take(&mdata->sem_http_action_response, K_SECONDS(120));
    // k_sleep(K_SECONDS(2));
    // LOG_ERR("-------> 2");
    // k_sleep(K_SECONDS(2));
    // LOG_ERR("-------> 3");

    // ret = k_sem_take(&http_action, K_SECONDS(120));
    if (ret < 0) {
		LOG_ERR("Timeout waiting for +HTTPACTION");
        goto out;
    }

    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
    if (ret != 0) {
        LOG_DBG("Failed to send data");
    }

out:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);

    return ret;
}

// struct http_last_action_result* modem_http_last_action()
// {
//     return &http_last_action_result;
// }



// uint8_t _http_tmp_buf[5000];
// size_t _http_tmp_buf_length;

static K_SEM_DEFINE(http_read_sem, 0, 1);


static struct http_data {
    uint8_t *recv_buf;
    size_t recv_buf_len;
    size_t recv_buf_goted_data;
} http_data;

static int on_cmd_sockread_common(struct modem_cmd_handler_data *data, uint16_t len)
{
    int ret, i;
	int socket_data_length;
	int bytes_to_skip;

    // LOG_DBG("on_cmd_sockread_common %d bytes", len);

    if (!len) {
		LOG_ERR("Invalid length, Aborting!");
		return -EAGAIN;
	}

    /* Make sure we still have buf data */
	if (!data->rx_buf) {
		LOG_ERR("Incorrect format! Ignoring data!");
		return -EINVAL;
	}

    socket_data_length = find_len(data->rx_buf->data);
    // LOG_DBG("socket_data_length = %d", socket_data_length);

    /* No (or not enough) data available on the socket. */
	bytes_to_skip = digits(socket_data_length) + 2 + 4;
	if (socket_data_length <= 0) {
		LOG_ERR("Length problem (%d).  Aborting!", socket_data_length);
		return -EAGAIN;
	}

    /* check to make sure we have all of the data. */
	if (net_buf_frags_len(data->rx_buf) < (socket_data_length + bytes_to_skip)) {
		// LOG_ERR("     -> TODO: Not enough data -- wait! %d < (%d + %d)",
        //     net_buf_frags_len(data->rx_buf), socket_data_length, bytes_to_skip
        // );
		return -EAGAIN;
	}
    // LOG_ERR("TODO: catched data %d", socket_data_length);

    /* Skip "len" and CRLF */
	bytes_to_skip = digits(socket_data_length) + 2;
	for (i = 0; i < bytes_to_skip; i++) {
		net_buf_pull_u8(data->rx_buf);
	}

    if (!data->rx_buf->len) {
		data->rx_buf = net_buf_frag_del(NULL, data->rx_buf);
	}

    ret = net_buf_linearize(
        &http_data.recv_buf[http_data.recv_buf_goted_data],
        http_data.recv_buf_len - http_data.recv_buf_goted_data,
		data->rx_buf, 0, (uint16_t)socket_data_length);
	data->rx_buf = net_buf_skip(data->rx_buf, ret);
	// _http_tmp_buf_length = ret;
	if (ret != socket_data_length) {
		LOG_ERR("Total copied data is different then received data!"
			" copied:%d vs. received:%d", ret, socket_data_length);
		ret = -EINVAL;
	}
    http_data.recv_buf_goted_data += socket_data_length;

	return ret;
}

/* Handler: Read HTTP data */
// +HTTPREAD: <data_len> - Next Data packet
// +HTTPREAD: 0 - Done
// <data>
MODEM_CMD_DEFINE(on_cmd_http_readdata)
{
    size_t data_len = ATOI(argv[0], 0, "data_len");
    LOG_ERR("data_len = %d", data_len);
    if(data_len == 0) {
        k_sem_give(&http_read_sem);
        return 0;
        // return on_cmd_http_readdata_done(data, len);
    } else {
        return on_cmd_sockread_common(data, len);
    }
}


// +HTTPREAD: 0 - Done
// MODEM_CMD_DEFINE(on_cmd_http_readdata_done)
// {
//     // LOG_ERR("  >>>> TODO: HTTP data done. Touch semaphore.");
//     k_sem_give(&http_read_sem);
//     // modem_cmd_handler_set_error(data, 0);
//     // k_sem_give(&mdata.sem_response);
// 	return 0;
// }

// AT+HTTPREAD=<byte_size>
// int modem_http_read_data(struct modem_data *mdata, void *buf, size_t len, int flags, size_t *goted_data)
int simcom_http_read(struct modem_data *mdata, void *response, size_t response_size, size_t *response_data_size)
{
    char   sendbuf[sizeof("AT+HTTPREAD=#######")] = {0};
	int    ret;

    /* Modem command to read the data. */
	struct modem_cmd data_cmd[] = {
        MODEM_CMD("+HTTPREAD: ", on_cmd_http_readdata, 1U, ""),
        // MODEM_CMD("+HTTPREAD: DATA,", on_cmd_http_readdata, 0U, ""),
        // MODEM_CMD("+HTTPREAD: 0", on_cmd_http_readdata_done, 0U, ""),
    };

    if (!response || response_size == 0) {
		// errno = EINVAL;
		return -1;
	}

    snprintk(sendbuf, sizeof(sendbuf), "AT+HTTPREAD=%zd", response_size);

    // TODO: I don't like this. I want to use http_data from modem_data
    (void) memset(&http_data, 0, sizeof(http_data));
	http_data.recv_buf     = response;
	http_data.recv_buf_len = response_size;
	http_data.recv_buf_goted_data = 0;

    /* Tell the modem to give us data (AT+HTTPREAD=data_len). */
    // LOG_ERR("TODO: Reset semaphore");
    k_sem_reset(&http_read_sem);

	ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
			     data_cmd, ARRAY_SIZE(data_cmd), sendbuf, &mdata->sem_response,
			     K_SECONDS(30), MODEM_NO_UNSET_CMDS);
	if (ret < 0) {
        LOG_ERR("modem_cmd_send_ext error. What? Why?");
		errno = -ret;
		ret = -1;
		goto exit;
	}

    // LOG_ERR("TODO: Wait semaphore");
    ret = k_sem_take(&http_read_sem, K_SECONDS(2));
    // LOG_ERR("TODO: Done semaphore");

	if (ret < 0) {
		LOG_ERR("Timeout waiting for HTTPREAD");
        goto exit;
    }

    *response_data_size = http_data.recv_buf_goted_data;
    // LOG_ERR("TODO: what we read (%d bytes)?", http_data.recv_buf_goted_data);
    // LOG_HEXDUMP_DBG(http_data.recv_buf, http_data.recv_buf_goted_data, "http buf");

exit:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);
    return ret;
}

#if 0
static int simcom_http_data(struct modem_data *mdata, const char *data, size_t data_size)
{
    int ret;
    char sendbuf[sizeof("AT+HTTPDATA=##,######")] = {0};

    snprintk(sendbuf, sizeof(sendbuf), "AT+HTTPDATA=%zd,%zd", data_size, 30000);

    // TODO: Add wait for "DOWNLOAD" or "ERROR"

    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, sendbuf, &mdata->sem_response, K_SECONDS(30));
    if(ret < 0) {
        LOG_ERR("modem_cmd_send error");
        goto exit;
    }

    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, data, data_size, NULL, &mdata->sem_response, K_SECONDS(30));
    if(ret < 0) {
        LOG_ERR("modem_cmd_send error");
        goto exit;
    }

exit:
    return ret;
}
#endif

int simcom_http(struct modem_data *mdata,
    const struct http_config *config,
    struct http_last_action_result *result,
    const char *url
    )
{
    int ret = 0;

    if(config->content_type != NULL) {
        ret = modem_http_set_accept_type(mdata, config->content_type);
        if(ret < 0) {
            LOG_ERR("modem_http_set_accept_type error");
            goto exit;
        }
    }
    if(config->accept_type != NULL) {
        ret = modem_http_set_content_type(mdata, config->accept_type);
        if(ret < 0) {
            LOG_ERR("modem_http_set_content_type error");
            goto exit;
        }
    }
    if(config->cid >= 0) {
        ret = modem_http_set_ssl_cfg(mdata, config->cid);
        if(ret < 0) {
            LOG_ERR("simcom_http_set_cid error");
            goto exit;
        }
    }

    ret = modem_http_set_url(mdata, url);
    if(ret < 0) {
        LOG_ERR("modem_http_set_url error");
        goto exit;
    }

    if(config->user_data != NULL) {
        ret = modem_http_set_user_data(mdata, config->user_data);
        if(ret < 0) {
            LOG_ERR("modem_http_set_user_data error");
            goto exit;
        }
    }

    // modem_http_set_user_data(mdata, "Host: het.navi.cc");

    #if 0
    if((body != NULL) && (data_size > 0)) {
        ret = simcom_http_data(mdata, body, data_size);
        if(ret < 0) {
            LOG_ERR("simcom_http_data error");
            goto exit;
        }
        // LOG_ERR("TODO: Send body");
        // LOG_HEXDUMP_DBG(body, data_size, "body");
        // ret = modem_http_send_data(mdata, body, data_size);
        // if(ret < 0) {
        //     LOG_ERR("modem_http_send_data error");
        //     goto exit;
        // }
    }
    #endif

    ret = modem_http_doit(mdata, config->method);

    // struct http_last_action_result* res = modem_http_last_action(mdata);
    // if(ret < 0) {
    //     LOG_ERR("Error GET for addlog. (%d)", ret);
    //     goto exit;
    // }

    if(result != NULL) {
        memcpy(result, &http_last_action_result, sizeof(struct http_last_action_result));
    }

    // if(res->status_code != 200) {
    //     LOG_ERR("Error GET for addlog. (STATUS: %d)", res->status_code);
    //     ret = -1;
    //     goto exit;
    // }

exit:
    return ret;
}



static unsigned char _hex2char(unsigned char value)
{
	value &= 0x0F;
	return (value <= 9) ? (value + '0') : (value + 'A' - 10);
}

static unsigned char lsb2ch(unsigned char value)
{
	return _hex2char(value);
}

static unsigned char msb2ch(unsigned char value)
{
	return _hex2char(value >> 4);
}

struct emitter {
	char *ptr;
	int len;
};

static int sprintf_out(int c, struct emitter *p)
{
	if (p->len > 1) { /* need to reserve a byte for EOS */
        if(c == ' ') {
            *(p->ptr) = '+';
		    p->ptr += 1;
		    p->len -= 1;
        } else if(
            (c > 127)
            || (c <= 0x1F) // Все до пробела
            || (strchr("\"#$%&+,/:;<=>?@[\\]^`{|}", c) != NULL)
        ) {
            if(p->len < 3) {
                return 0;
            }
            *(p->ptr) = '%';
            p->ptr += 1;
            *(p->ptr) = msb2ch(c);
            p->ptr += 1;
            *(p->ptr) = lsb2ch(c);
            p->ptr += 1;
            p->len -= 3;
        } else {
		    *(p->ptr) = c;
		    p->ptr += 1;
		    p->len -= 1;
        }
	}
	return 0; /* indicate keep going so we get the total count */
}


int snprintf_uri(char *ZRESTRICT str, size_t len,
	     const char *ZRESTRICT format, ...)
{
	va_list vargs;

	struct emitter p;
	int     r;
	char    dummy;

	if (len == 0) {
		str = &dummy; /* write final NUL to dummy, can't change *s */
	}

	p.ptr = str;
	p.len = (int) len;

	va_start(vargs, format);
	r = cbvprintf(sprintf_out, (void *) (&p), format, vargs);
	va_end(vargs);

	*(p.ptr) = 0;
	return r;
}


// void gsmURI(const char *format, ...)
// {
// 	va_list arg_list;
//     va_start(arg_list, format);
//     /*int length = */cbvprintf(out_uri_func, (void *)NULL, format, arg_list);
//     va_end(arg_list);
// }


#if 0
#define TPF(_n_) _n_

// TODO: Hardcoded for now
// #define gsm_server "point.new.navi.cc"
#define gsm_server "point.fx.navi.cc"
// #define gsm_extra ""
const char* gsm_extra = "";


int gsm_http_url(char *url)
{
	LOG_DBG("URL(%s/%s)", url, gsmImei());
	int gsm_csq = getCsq();
	// int gsm_csq  = 0;
	// return 0;
	gsmCmdPre(
		"AT+HTTPPARA=\"URL\",\"http://%s/%s"
		"?imei=%s"
		"&csq=%d"
        "&vout=12345&vin=3600"  // TODO: Проверить что ничего не крашится без этого поля
		// "&data-state=%s"
		// TODO: Use it for debug on Fenix APP
        #ifdef PROTOCOL_FX
            "&state=%s"
            "&available=%s"
            "&next_session=%d"
            "&as=%d"
        #endif
		, TPF(gsm_server), url
		, gsmImei() //, (TPF(gsm_extra)[0]) ? "-" : "", (const char *)TPF(gsm_extra)
		, gsm_csq
		// , "---"	// data-state
        #ifdef PROTOCOL_FX
            , tracker_state_as_string()
            , tracker_states_available()
            , tracker_next_session()
            , tracker_autosleep_timer()
        #endif
	);
	return 0;
}

int gsm_http_urlend()
{
	uart_putch('\"'); uart_putch('\r');
	// gsm_putstr("\"\r\n");
	if(!gsmWok()){
		// gprs_fails++;
		return 101;
	}
	return 0;
}


bool gsm_http_doit(int method, int timeout, uint32_t *data_size)
{
	char* answer;
	int ret_method;
	int status;

    // if(timeout < 11) timeout = 11;
    // gsmCmdWok("AT+HTTPPARA=\"TIMEOUT\", %d", timeout-10);

    // TODO: Use tryes from ccore

    if(!gsmCmdWokTo(10, "AT+HTTPACTION=%d", (method == GSM_METHOD_GET) ? 0 : 1)) {
        // if(method != GSM_METHOD_GET) {
            return false;
        // }
    }
	// TODO: Use method

	// gsmCmd("AT+HTTPACTION=0");	// GET
	// Wait for +HTTPACTION:%d,%d,%ld", &method, &status, &length

	while(true) {
		if((answer = read_line(timeout)) != NULL) {
			int ret = sscanf(answer, "+HTTPACTION:%d,%d,%"SCNu32, &ret_method, &status, data_size);
			if(ret != 3) {
				LOG_ERR("Error +HTTPACTION: request answer [%s]", /*log_strdup(*/answer/*)*/);
                // TODO: Use unhandled gsm parser
				continue;
			}
			// uart_wait_exact("OK", 1);

			if(status != 200) {
				LOG_ERR("Server return code: %d", status);
				if(status >= 600) {
					// TODO: Restart GPRS and repeat
				}
				// 408 - TIMEOUT - Just repeat
				return false;
			}

			LOG_DBG("Server return data [%"PRIu32" bytes]", *data_size);
            // http_ans_len = length;

			return true;
		}
		return false;
	}

}

static unsigned char _hex2char(unsigned char value)
{
	value &= 0x0F;
	return (value <= 9) ? (value + '0') : (value + 'A' - 10);
}

static unsigned char lsb2ch(unsigned char value)
{
	return _hex2char(value);
}

static unsigned char msb2ch(unsigned char value)
{
	return _hex2char(value >> 4);
}

static int out_uri_func(int c, void *ctx)
{
    // uart_putch((uint8_t)c);

	if(c==0) return 0;
	if(
		(c > 127)
		|| (c <= 0x20) // Все до пробела включительно
		|| (strchr("\"#$%&+,/:;<=>?@[\\]^`{|}", c) != NULL)
	) {
		uart_putch('%');
		uart_putch(msb2ch(c));
		uart_putch(lsb2ch(c));
	} else {
		uart_putch((uint8_t)c);
	}

    return 0;
}

void gsmURI(const char *format, ...)
{
	va_list arg_list;
    va_start(arg_list, format);
    /*int length = */cbvprintf(out_uri_func, (void *)NULL, format, arg_list);
    va_end(arg_list);
}

unsigned long readlen;

/*
unsigned long gsm_http_server_answer_start(uint32_t data_size)
{
    char* gsm_answer;
	// int i;
//	ds.GSMWaitServerAnswer = 0;
	// gsm_delaym(16);
	gsmCmd("AT+HTTPREAD=%"PRIu32, data_size);

	// timeSetTimeout(_s(10));	// 10 секунды (данные уже в памяти, возможно 10 секунд даже много)
	while(1){
		// if(!gsmReadLine(0)){
        if((gsm_answer = read_line(10)) == NULL) {
			// DEBUG_PRINT(" GSM error: timeout.\r\n");
			// gsm_timeouterror++;
			return 105;
		}
		if(strncmp(gsm_answer, "+HTTPREAD:", 10) == 0){
			readlen = 0;
			sscanf(gsm_answer, "+HTTPREAD:%ld", &readlen);
			// i=10;
			// if(gsm_answer[i] == ' ') i++;
			// for(; (i<10+8) && isdigit(gsm_answer[i]); i++) readlen = readlen*10 + (gsm_answer[i]-'0');
			return 0;
		} else {
			// gsmUnhandledLine(gsm_answer);
            LOG_ERR("char* answer: %s", gsm_answer);
		}
	}
}
*/
#endif


// Wait +HTTPREADFILE: <errcode>
MODEM_CMD_DEFINE(on_cmd_http_read_file)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

    // http_last_action_result.success = true;
    int errcode = ATOI(argv[0], 0, "errcode");
    LOG_DBG("*> Got +HTTPREADFILE=%d", errcode );
    modem_cmd_handler_set_error(data, errcode);   //  -EIO
    // k_sem_give(&mdata->sem_response);
    k_sem_give(&mdata->sem_http_file_read);
	return 0;
}


int modem_simcom_http_fileread(const struct device *dev, const char* filename)
{
    int ret;

    struct modem_data *mdata = (struct modem_data *)dev->data;

#if 0
    // AT+HTTPPARA="URL","url"
    ret = modem_http_set_url(mdata, url);
    if(ret < 0) {
        LOG_ERR("modem_http_set_url error");
        goto out;
    }

    ret = modem_http_doit(mdata, HTTP_GET);
    if(ret < 0) {
        LOG_ERR("modem_http_doit error");
        goto out;
    }

#endif
    // AT+HTTPREADFILE="filename"

    char send_buf[sizeof("AT+HTTPREADFILE=\"file_name_file_name_file_name_\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+HTTPREADFILE=\"%s\"", filename);

    struct modem_cmd cmd = MODEM_CMD("+HTTPREADFILE:", on_cmd_http_read_file, 1U, "");

    // http_last_action_result.success = false;
    // k_sem_reset(&http_action);
    k_sem_reset(&mdata->sem_http_file_read);
    ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, send_buf, &mdata->sem_response, K_SECONDS(60), MODEM_NO_UNSET_CMDS);
    if(ret < 0) {
        LOG_ERR("Error HTTPREADFILE= command");
        goto out;
    }

	ret = k_sem_take(&mdata->sem_http_file_read, K_SECONDS(10));

    if (ret < 0) {
		LOG_ERR("Timeout waiting for +HTTPREADFILE");
        goto out;
    }

    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
    if (ret != 0) {
        LOG_ERR("Failed to read file");
    }

out:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);

    return 0;
}
