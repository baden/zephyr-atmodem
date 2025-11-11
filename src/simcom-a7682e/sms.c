#include <stdlib.h>
#include "sms.h"
// #include "lte/lte.h"
#include "ucs2.h"
#include "drivers/at-modem.h"
// #include "nvm/nvm.h"
#include "id.h"
#include "../utils.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

// #include "modem_cmd_handler.h"

/* Handler: TX Ready */
MODEM_CMD_DIRECT_DEFINE(on_cmd_tx_ready)
{
   	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
	k_sem_give(&mdata->sem_tx_ready);
    return len;
}

/* Handler: +CMGS: <mr> */
MODEM_CMD_DEFINE(on_cmd_cmgs_mr)
{
    // int mr = ATOI(argv[0], 0, "mr");
	return 0;
}

uint8_t modem_send_sms_stages = 0;

// AT+CMGF=1
static int modem_send_sms_text_format(struct modem_data *mdata)
{
    return simcom_cmd(mdata, "AT+CMGF=1", K_SECONDS(2));
}

static int modem_send_sms_select_message_service0(struct modem_data *mdata)
{
    return simcom_cmd(mdata, "AT+CSMS=0", K_SECONDS(2));
}

// static int modem_send_sms_select_message_service1()
// {
//     return modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, "AT+CSMS=1", &mdata.sem_response, K_SECONDS(2));
// }

static int modem_send_sms_preffered_message_storage_me(struct modem_data *mdata)
{
    return simcom_cmd(mdata, "AT+CPMS=\"ME\",\"ME\",\"ME\"", K_SECONDS(2));
}

/*static*/ int modem_char_set(struct modem_data *mdata, t_sms_charset charset, t_sms_flags flags)
{
	char *send_buf = "AT+CSCS=\"IRA\";+CSMP=17,167,0,16";

    switch(charset){
        case GSM_CHARSET_GSM:
            // gsm_cmd_wok("AT+CSCS=\"GSM\"\x3B+CSMP=17,169,0,241");
            if(flags & SMS_FLAGS_FLASH){
                send_buf = "AT+CSCS=\"GSM\";+CSMP=17,167,0,16";
            } else {
                send_buf = "AT+CSCS=\"GSM\";+CSMP=17,167,0,0";
            }
            break;
        case GSM_CHARSET_IRA:
            // gsm_cmd_wok("AT+CSCS=\"HEX\"\x3B+CSMP=17,169,0,241");
            if(flags & SMS_FLAGS_FLASH){
                send_buf = "AT+CSCS=\"IRA\";+CSMP=17,167,0,16";
            } else {
                send_buf = "AT+CSCS=\"IRA\";+CSMP=17,167,0,0";
            }
            break;
        case GSM_CHARSET_UCS2:
            if(flags & SMS_FLAGS_FLASH){
                send_buf = "AT+CSCS=\"UCS2\"\x3B+CSMP=17,167,0,24";
            } else {
                send_buf = "AT+CSCS=\"UCS2\"\x3B+CSMP=17,167,0,25";
            }
            break;
    }

    return simcom_cmd(mdata, send_buf, K_SECONDS(2));
}

// Convert each code to 16-bit UCS2
// void utf2ucs2(const unsigned char* message, unsigned char* ucs2)
// {
//     while(*message){
//         unsigned char p = *message++;
//         *ucs2
//     }
//     *ucs2 = 0;

// }

// Функція для перетворення символу UTF-8 на UCS2
#if 0
unsigned short utf8_to_ucs2(const unsigned char* utf8, int* len)
{
    unsigned short ucs2 = 0;
    if((*utf8 & 0x80) == 0){
        ucs2 = *utf8;
        *len = 1;
    } else if((*utf8 & 0xE0) == 0xC0){
        ucs2 = (*utf8 & 0x1F) << 6;
        ucs2 |= (*(utf8+1) & 0x3F);
        *len = 2;
    } else if((*utf8 & 0xF0) == 0xE0){
        ucs2 = (*utf8 & 0x0F) << 12;
        ucs2 |= (*(utf8+1) & 0x3F) << 6;
        ucs2 |= (*(utf8+2) & 0x3F);
        *len = 3;
    } else if((*utf8 & 0xF8) == 0xF0){
        ucs2 = (*utf8 & 0x07) << 18;
        ucs2 |= (*(utf8+1) & 0x3F) << 12;
        ucs2 |= (*(utf8+2) & 0x3F) << 6;
        ucs2 |= (*(utf8+3) & 0x3F);
        *len = 4;
    }
    return ucs2;
}
#else
// Функція для перетворення символу UTF-8 на UCS-2
uint16_t utf8_to_ucs2(const uint8_t* utf8, int* len) {
    uint16_t ucs2 = 0;

    if ((*utf8 & 0x80) == 0) {
        // 1-байтовий символ (ASCII)
        ucs2 = *utf8;
        *len = 1;
    }
    else if ((*utf8 & 0xE0) == 0xC0) {
        // 2-байтовий символ (U+0080 - U+07FF)
        if ((utf8[1] & 0xC0) != 0x80) goto error; // Перевірка продовження
        ucs2 = ((utf8[0] & 0x1F) << 6) | (utf8[1] & 0x3F);
        *len = 2;
    }
    else if ((*utf8 & 0xF0) == 0xE0) {
        // 3-байтовий символ (U+0800 - U+FFFF)
        if ((utf8[1] & 0xC0) != 0x80 || (utf8[2] & 0xC0) != 0x80) goto error;
        ucs2 = ((utf8[0] & 0x0F) << 12) | ((utf8[1] & 0x3F) << 6) | (utf8[2] & 0x3F);
        *len = 3;
    }
    else {
        // 4-байтовий символ (U+10000 - U+10FFFF) → UCS-2 не підтримує!
        goto error;
    }

    return ucs2;

error:
    *len = 1;
    return 0x003F; // Символ "?" (U+003F) як заміна некоректного UTF-8
}
#endif

// Convert utf-8 string to UCS-2 in HEX format
// Returns the number of bytes written to the buffer
size_t utf8_to_ucs2_string(const char* src, char* buf, size_t buf_size)
{
    size_t len = 0;
    while(*src){
        int utf8_len;
        unsigned short ucs2 = utf8_to_ucs2((const unsigned char*)src, &utf8_len);
        if(len + 4 > (buf_size-1)) break;
        len += snprintk(buf+len, buf_size-len, "%04X", ucs2);
        src += utf8_len;
    }
    buf[len] = 0;
    return len;
}

int simcom_send_sms(struct modem_data *mdata, const char *message, const char *phone)
{
    int  ret;
	char send_buf[sizeof("AT+CMGS=\"############################################################\"")] = {0};
	char ctrlz = 0x1A;

    char ucs2_buf[256];

    t_sms_charset charset = GSM_CHARSET_GSM;
    const char *m  = message;
    while(*m){
        if(*m > 127){
            charset = GSM_CHARSET_UCS2;
            break;
        }
        m++;
    }

    modem_send_sms_stages = 1;
    modem_send_sms_select_message_service0(mdata);
    modem_send_sms_preffered_message_storage_me(mdata);
    modem_send_sms_text_format(mdata);
    // TODO: For now, only 7bit charset supported

    // TODO: Restore me
    // #warning Restore me
    modem_char_set(mdata, charset, SMS_FLAGS_DEFAULT);

	// if (buf_len > MDM_MAX_DATA_LENGTH) {
	// 	buf_len = MDM_MAX_DATA_LENGTH;
	// }

	/* Create a buffer with the correct params. */
	// mdata.sock_written = buf_len;
    // if GSM_CHARSET_GSM phone number should be in UCS-2 HEX format
    if(charset == GSM_CHARSET_UCS2) {
        char phone_ucs2[48];
        utf8_to_ucs2_string(phone, phone_ucs2, sizeof(phone_ucs2));
        snprintk(send_buf, sizeof(send_buf), "AT+CMGS=\"%s\"", phone_ucs2);
    } else {
	    snprintk(send_buf, sizeof(send_buf), "AT+CMGS=\"%s\"", phone);
    }

	/* Setup the locks correctly. */
	k_sem_take(&mdata->cmd_handler_data.sem_tx_lock, K_FOREVER);
	k_sem_reset(&mdata->sem_tx_ready);

	/* Send the Modem command. */
	ret = modem_cmd_send_nolock(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
				    NULL, 0U, send_buf, NULL, K_NO_WAIT);
	if (ret < 0) {
        modem_send_sms_stages = 2;
		goto exit;
	}
    modem_send_sms_stages = 3;

    struct modem_cmd handler_cmds[] = {
		MODEM_CMD_DIRECT(">", on_cmd_tx_ready),
        MODEM_CMD("+CMGS: ", on_cmd_cmgs_mr, 1U, ""),   // Ignoring?
	};

	/* set command handlers */
	ret = modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
					    handler_cmds, ARRAY_SIZE(handler_cmds),
					    true);
	if (ret < 0) {
        modem_send_sms_stages = 4;
		goto exit;
	}
    modem_send_sms_stages = 5;

    /* Wait for '>' */
    ret = k_sem_take(&mdata->sem_tx_ready, K_MSEC(5000));
    if (ret < 0) {
        /* Didn't get the data prompt - Exit. */
        LOG_DBG("Timeout waiting for tx");
        modem_send_sms_stages = 6;
        goto exit;
    }
    modem_send_sms_stages = 7;

    /* Write all data on the console and send CTRL+Z. */
    if(charset == GSM_CHARSET_UCS2) {
            utf8_to_ucs2_string(message, ucs2_buf, sizeof(ucs2_buf));
            mdata->mctx.iface.write(&mdata->mctx.iface, ucs2_buf, strlen(ucs2_buf));
    //     char *ucs2 = (char*)k_malloc(strlen(message)*4 + 1);
    //     if(ucs2 == NULL){
    //         LOG_ERR("Failed to allocate memory for UCS2");
    //         ret = -ENOMEM;
    //         goto exit;
    //     }
    //     ucs2_to_gsm7bit(message, ucs2);
    //     mdata->mctx.iface.write(&mdata->mctx.iface, ucs2, strlen(ucs2));
    //     k_free(ucs2);
    } else {
        mdata->mctx.iface.write(&mdata->mctx.iface, message, strlen(message));
    }
	// mdata->mctx.iface.write(&mdata->mctx.iface, message, strlen(message));
	mdata->mctx.iface.write(&mdata->mctx.iface, &ctrlz, 1);

	/* Wait for 'SEND OK' or 'SEND FAIL' */
	k_sem_reset(&mdata->sem_response);
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(150));
	if (ret < 0) {
        modem_send_sms_stages = 8;
		LOG_DBG("No send response");
		goto exit;
	}
    modem_send_sms_stages = 9;

	ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
	if (ret != 0) {
		LOG_DBG("Failed to send SMS");
        modem_send_sms_stages = 10;
        goto exit;
	}
    LOG_DBG("SMS send ok");
    modem_send_sms_stages = 11;

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

// TODO: Use dynamic allocation
#define DIRECT_CMD_ANSWER_MAX_LENGTH 128
static char direct_cmd_answer[DIRECT_CMD_ANSWER_MAX_LENGTH];
/* Handler: <manufacturer> */
MODEM_CMD_DEFINE(on_cmd_direct_cmd)
{
    size_t out_len = net_buf_linearize(direct_cmd_answer,
                       sizeof(direct_cmd_answer) - 1,
                       data->rx_buf, 0, len);
    direct_cmd_answer[out_len] = '\0';
    LOG_INF("Direct CMD answer: %s", /*log_strdup(*/direct_cmd_answer/*)*/);
    return 0;
}

int modem_send_cmd(struct modem_data *mdata, const char *command, const char *sender)
{
    // const char *send_buf = "AT+HTTPINIT";
    // return modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, send_buf, &mdata.sem_response, K_SECONDS(30));

    struct setup_cmd direct_cmds = SETUP_CMD(command, "", on_cmd_direct_cmd, 0U, "");

    direct_cmd_answer[0] = 0;
    modem_cmd_handler_setup_cmds(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
					   &direct_cmds, 1U,
					   &mdata->sem_response, K_SECONDS(5));

    if(direct_cmd_answer[0] != 0) {
        simcom_send_sms(mdata, direct_cmd_answer, sender);
    }
    return 0;
}

//bool sms_body_inUCS2;

#if 0
static int intlength(int v)
{
    if(v >= 1000) return 4;
    if(v >= 100) return 3;
    if(v >= 10) return 2;
    return 1;
}
#endif

/* Handler: +CMGR: ... hard to parse */
// Fake delim on timetamp 21/11/26,09:12:02+08
// +CMGR: "REC READ","7512110511811511697114","","21/11/26,09:12:02+08",208,96,0,8,"+380672020020",145,67
//        0          1                        2  3         4            5   6  7 8 9               10  11

// TODO: dirty hack. Треба це зробити нормально

#define LTE_MODEM_DEVICE DEVICE_DT_GET(DT_ALIAS(lte))

static int sms_incoming_handler(void *payload, size_t payload_size)
{
   	// struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);


    // TODO: Use internal API layer
    const struct device *dev = LTE_MODEM_DEVICE;
    struct modem_data *mdata = dev->data;

    struct sms_payload *sms = (struct sms_payload *)payload;

    LOG_ERR("SMS incoming handler [%s] <- [%s]", sms->phone, sms->message);

    if(mdata->event_cb) {
        mdata->event_cb(ATMODEM_EVENT_INCOMING_SMS, sms, sizeof(struct sms_payload));
    }
    return 0;
}

// +CMGR: "REC READ","002B003300380030003600370039003300330032003300330032","","22/11/01,00:44:53+08",145,4,0,0,"002B003300380030003600370032003000320030003000320030",145,6
// +CMGR: "REC UNREAD","002B003300380030003600370039003300330032003300330032","","25/02/11,12:54:21+8",145,17,0,25,"002B003300380030003600370032003000320030003000310030",145,4

// Параметрів на справді 11, але дата містить кому (параметри номер 3 і 4)

/*
// +CMGR:
0 "REC UNREAD",
1 "002B003300380030003600370039003300330032003300330032",
2 "",
3 "25/02/11,
4 12:54:21+8",
5 145,
6 17,
7 0,
8 25,
9 "002B003300380030003600370032003000320030003000310030",
10 145,
11 4
*/

#if 0
/**
 * Check if given char sequence is crlf.
 *
 * @param c The char sequence.
 * @param len Total length of the fragment.
 * @return @c true if char sequence is crlf.
 *         Otherwise @c false is returned.
 */
static bool is_crlf(uint8_t *c, uint8_t len)
{
	/* crlf does not fit. */
	if (len < 2) {
		return false;
	}

	return c[0] == '\r' && c[1] == '\n';
}
#endif

#if 0
/**
 * Find terminating crlf in a netbuffer.
 *
 * @param buf The netbuffer.
 * @param skip Bytes to skip before search.
 * @return Length of the returned fragment or 0 if not found.
 */
static size_t net_buf_find_crlf(struct net_buf *buf, size_t skip)
{
	size_t len = 0, pos = 0;
	struct net_buf *frag = buf;

	/* Skip to the start. */
	while (frag && skip >= frag->len) {
		skip -= frag->len;
		frag = frag->frags;
	}

	/* Need to wait for more data. */
	if (!frag) {
		return 0;
	}

	pos = skip;

	while (frag && !is_crlf(frag->data + pos, frag->len - pos)) {
		if (pos + 1 >= frag->len) {
			len += frag->len;
			frag = frag->frags;
			pos = 0U;
		} else {
			pos++;
		}
	}

	if (frag && is_crlf(frag->data + pos, frag->len - pos)) {
		len += pos;
		return len - skip;
	}

	return 0;
}
#endif

struct csms_data_s {
	uint8_t csms_ref;
	uint8_t csms_idx;
	uint8_t csms_tot;
};

int pdu_msg_extract(char *pdu_buffer, struct csms_data_s *csms_data)
{
    LOG_ERR("PDU: %s", pdu_buffer);
    return 0;
}

#if 0
MODEM_CMD_DEFINE(on_cmd_cmgr2)
{
	char pdu_buffer[360];
	size_t out_len, sms_len, param_len;

    LOG_ERR("CMGR handler (len=%d, argc=%d)", len, argc);

	/* Get the length of the "length" parameter.
	 * The last parameter will be stuck in the netbuffer.
	 * It is not the actual length of the trailing pdu so
	 * we have to search the next crlf.
	 */
	param_len = net_buf_find_crlf(data->rx_buf, 0);
	if (param_len == 0) {
		LOG_ERR("No <CR><LF>");
		return -EAGAIN;
	}

	/* Get actual trailing pdu len. +2 to skip crlf. */
	sms_len = net_buf_find_crlf(data->rx_buf, param_len + 2);
	if (sms_len == 0) {
		return -EAGAIN;
	}

	/* Skip to start of pdu. */
	data->rx_buf = net_buf_skip(data->rx_buf, param_len + 2);
	out_len = net_buf_linearize(pdu_buffer, sizeof(pdu_buffer) - 1, data->rx_buf, 0, sms_len);
	pdu_buffer[out_len] = '\0';

	data->rx_buf = net_buf_skip(data->rx_buf, sms_len);

	return pdu_msg_extract(pdu_buffer, NULL);
}
#endif

#if 0
// Ця функція як оригінальна net_buf_linearize, але вона перетворює UCS2 в UTF8
static size_t net_buf_linearize_utf8(char *dst, size_t dst_len, const struct net_buf *src,
    size_t offset, size_t len)
{
    const struct net_buf *frag;
    size_t to_copy;
    size_t copied;
    size_t utf8_copied;

    // len = MIN(len, dst_len);

    frag = src;

    /* find the right fragment to start copying from */
    while (frag && offset >= frag->len) {
        offset -= frag->len;
        frag = frag->frags;
    }

    /* traverse the fragment chain until len bytes are copied */
    copied = 0;
    utf8_copied = 0;
    while (frag && len > 0) {
        to_copy = MIN(len, frag->len - offset);
        // Шматок даних (frag->data + offset) розміром to_copy
        // memcpy((uint8_t *)dst + copied, frag->data + offset, to_copy);
        // LOG_ERR("UCS2: [%s]", frag->data + offset);
        LOG_HEXDUMP_ERR(frag->data + offset, to_copy, "UCS2");
        // fake copy
        size_t utf8_piece = 1;
        memset(dst + utf8_copied, 0x20, utf8_piece);
        utf8_copied += utf8_piece;

        copied += to_copy;

        /* to_copy is always <= len */
        len -= to_copy;
        frag = frag->frags;

        /* after the first iteration, this value will be 0 */
        offset = 0;
    }

    return copied;
}
#endif

// TODO: Чомусь я отримав отак:
/*
[141:39:24.085,000] <err> cdc_acm_echo: 2:+CMTI: "ME",4
[141:39:24.194,000] <inf> cdc_acm_echo: 1:AT+CSCS="UCS2";+CSMP=17,167,0,25
[141:39:24.202,000] <err> cdc_acm_echo: 2:AT+CSCS="UCS2";+CSMP=17,167,0,25
[141:39:24.291,000] <err> cdc_acm_echo: 2:OK
[141:39:24.294,000] <inf> cdc_acm_echo: 1:AT+CMGR=4
[141:39:24.296,000] <err> cdc_acm_echo: 2:AT+CMGR=4
[141:39:24.308,000] <err> cdc_acm_echo: 2:+CMGR: "REC UNREAD","002B003300380030003600370039003300330032003300330032","","25/03/19,16:13:35+8"
[141:39:24.310,000] <err> cdc_acm_echo: 2:004C0069006E006B
[141:39:24.310,000] <err> cdc_acm_echo: 2:OK
*/


MODEM_CMD_DEFINE(on_cmd_cmgr)
{
    int ret;

    struct sms_payload sms;
    char body[SEND_SMS_MAX_MESSAGE_LENGTH*4 + 1];
    char phone_ucs2[16*4 + 1];

    // LOG_ERR("CMGR handler (len: %d, argc: %d)", len, argc);
    // LOG_ERR("argv[0]: [%s]", argv[0]);

    // Ми отримали шось на кшталт:
    // "REC UNREAD","002B003300380030003600370039003300330032003300330032","","25/02/11,13:55:34+8",145,17,0,25,"002B003300380030003600370032003000320030003000310030",145,4
    // 1. Виділити довжину SMS
    // Це остання цифра в рядку

    char *start = argv[0];
    char *p = &(argv[0][len-1]);
    while(p > start && *p != ',') p--;
    // LOG_ERR("SMS body length: [%s]", p);
    // Значення після 11 коми - довжина SMS
    size_t sms_body_length = atoi(p+1);
    // LOG_ERR("SMS body length: %d", sms_body_length);

    // int skips = intlength(sms_body_length) + 2; // length value + 0x0A + 0x0D
    int skips = len + 2; // 0x0A + 0x0D

    if (net_buf_frags_len(data->rx_buf) < (sms_body_length*4 + skips + 2)) {
		// LOG_ERR("Not enough data -- wait!");
		return -EAGAIN;
	}

    // ucs2_to_ascii(sms.phone, argv[1], sizeof(sms.phone));
    // Треба знайти де в argv[0] номер телефону. Він десь пістя першої коми та в кавичках
    strcpy(sms.phone, "unknown");
    p = argv[0];
    while(*p && *p != ',') p++;
    if(*p == ',') p++; else goto nophone;
    if(*p == '"') p++; else goto nophone;

    // Тепер треба знайти кінець номеру телефону (закриваюча кавичка)
    char *q = p;
    while(*q && *q != '"') q++;
    if(*q == '"') {
        size_t tlen = MIN(q-p, sizeof(phone_ucs2)-1);
        memcpy(phone_ucs2, p, tlen);
        phone_ucs2[tlen] = 0;
        LOG_HEXDUMP_ERR(phone_ucs2, tlen+1, "Phone UCS2");
        // tlen = MIN(tlen/4, sizeof(sms.phone)-1);
        ucs2_to_ascii(sms.phone, phone_ucs2, sizeof(sms.phone));
        LOG_HEXDUMP_ERR(sms.phone, tlen, "Phone UTF8");
    }

nophone:
    // Поки встановимо фейковий номер
    // strcpy(sms.phone, "+380679332332");

    // LOG_ERR("Sender: [%s]", sms.phone);

    // Skip "+CMGR..." URC and 0x0A, 0x0D
    data->rx_buf = net_buf_skip(data->rx_buf, skips);

    // TODO:
    // Я б хотів позбавитись цого, і задіяти декодування в utf8 прям з потоку.
    // За для економії пам'яті стеку
    // Як спрощеня альтернатива, можна порізати, скажемо по 32 байти.

    // ret = net_buf_linearize_utf8(sms.message, sizeof(sms.message)-1, data->rx_buf, 0, (uint16_t)sms_body_length*4);
    ret = net_buf_linearize(body, sizeof(body)-1, data->rx_buf, 0, (uint16_t)sms_body_length*4);
    // data[sizeof(data)-1] = 0;
    if (ret != sms_body_length*4) {
        // Цього ніколи не має траплятись
		LOG_ERR("Total copied data is different then received data!"
			" copied:%d vs. received:%d",
			ret, sms_body_length*4);
		goto exit;
		ret = -EINVAL;
	}
    data->rx_buf = net_buf_skip(data->rx_buf, ret+2);   // Skip data and last 0x0A, 0x0D

    body[ret] = 0;
    size_t tlen = MIN(sms_body_length+1, sizeof(sms.message));
    // ucs2_to_ascii(sms.message, body, tlen);
    ucs2_to_utf8(sms.message, body, tlen);

    // LOG_ERR("SMS body length: %d", sms_body_length);

    // LOG_HEXDUMP_ERR(sms.message, sms_body_length+1, "SMS body");
    // LOG_HEXDUMP_ERR(body, sms_body_length*4+1, "SMS body");

    #if 0
    size_t tlen = MIN(sms_body_length+1, sizeof(sms.message));
    ucs2_to_ascii(sms.message, body, tlen);

    LOG_HEXDUMP_ERR(sms.message, sms_body_length+1, "SMS body");
    #endif

    atmodem_run(LTE_MODEM_DEVICE, sms_incoming_handler, &sms, sizeof(sms)); // TODO: Cut to text size

    #if defined(CONFIG_DEBUG_SMS)
        LOG_ERR("Remove me");
        // size_t frags_len = net_buf_frags_len(data->rx_buf);
        naviccAddLogSimple("1/2-%s:%s:%s:%s:%s:%s"
            , argv[0], argv[1], argv[2], argv[3], argv[4], argv[5], argv[6]
        );
        naviccAddLogSimple("2/2-%s:%s:%s:%s:%s:%s"
            , argv[7], argv[8], argv[9], argv[10], argv[11], argv[12]
        );
    #endif

    ret = 0;

exit:
	/* Indication only sets length to a dummy value. */
	// packet_size = modem_socket_next_packet_size(&mdata.socket_config, sock);
	// modem_socket_packet_size_update(&mdata.socket_config, sock, -packet_size);
	return ret;
}

static int modem_check_sms(struct modem_data *mdata, int index)
{
    // TODO: gsmCharSet(GSM_CHARSET_IRA, SMS_FLAGS_DEFAULT);
    // modem_char_set(GSM_CHARSET_IRA, SMS_FLAGS_DEFAULT);
    // #warning Restore me
    modem_char_set(mdata, GSM_CHARSET_UCS2, SMS_FLAGS_DEFAULT);

    int  ret;
	char send_buf[sizeof("AT+CMGR=##")] = {0};

    snprintk(send_buf, sizeof(send_buf), "AT+CMGR=%d", index);
    static const struct modem_cmd cmds[] = {
        // Параметрів на справді 11, але дата містить кому (параметри номер 3 і 4)
        // MODEM_CMD("+CMGR: ", on_cmd_cmgr, 12U, ",")
        MODEM_CMD("+CMGR: ", on_cmd_cmgr, 1U, "")
        // MODEM_CMD("+CMGR: ", on_cmd_cmgr, 2U, "\r"),
    };
    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, cmds, ARRAY_SIZE(cmds), send_buf, &mdata->sem_response, K_SECONDS(2));
    if (ret < 0) {
        LOG_ERR("Error reading SMS: (%d)", ret);
        // naviccAddLogSimple("Error reading SMS");
        goto exit;
    }

exit:
    return ret;
}

static int modem_delete_sms(struct modem_data *mdata, int index)
{
	char send_buf[sizeof("AT+CMGD=##,##")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMGD=%d,1", index);
    return simcom_cmd(mdata, send_buf, K_SECONDS(2));
}

static int check_sms(struct modem_data *mdata, int index)
{
    if(modem_check_sms(mdata, index) >= 0 ){
        modem_delete_sms(mdata, index);
    }
    return 0;
}

int simcom_sms_read(void *payload, size_t payload_size)
{
    const struct device *dev = LTE_MODEM_DEVICE;
    struct modem_data *mdata = dev->data;
    int index = *(int*)payload;

    if(index == -1) {
        for(unsigned int i=0; i<10; i++) check_sms(mdata, i);
    } else {
        check_sms(mdata, index);
    }

    // Expect lines
    // +CMGL:
    // +CMGL: 1,"STO UNSENT","+10011",,,145,4
    // smsCmdCheck(1);

    // Purge all readed messages
    // gsmCmdWokTo(10, "AT+CMGDA=\"DEL READ\"");
    return 0;
}

// void LteCheckSMS(int index)
// {
//     atmodem_run(LTE_MODEM_DEVICE, LteCheckSMS_handler, &index, sizeof(index));
// }


static int modem_set_sms_center(struct modem_data *mdata, const char* phone)
{
    char send_buf[sizeof("AT+CSCA=\"################\"")] = {0};
    if(phone == NULL) return 0;
    if(phone[0] == 0) return 0;
    snprintk(send_buf, sizeof(send_buf), "AT+CSCA=\"%s\"", phone);
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

int simcom_setup_sms_center(struct modem_data *mdata)
{
    // Temporary disabled
    const char *sms_center = "";
    #if 0
    char lte_sms[NVM_LTE_SMS_LENGTH] = {0};
    int ret = nvm_load_value(NVM_LTE_SMS, lte_sms, NVM_LTE_SMS_LENGTH);
    if((ret > 0) && (strcmp(lte_sms, "*auto") != 0)) {
        lte_sms[NVM_LTE_SMS_LENGTH-1] = 0;
        sms_center = lte_sms;
    } else {
        char *op = query_operator(mdata);
        if(strstr(op, "KYIVSTAR") != NULL) {
            sms_center = "+380672021111";
        } else if(strstr(op, "lifecell") != NULL) {
            sms_center = "+380639010000";
        } else if(strstr(op, "AT&T") != NULL) {
            sms_center = "+13123149810";
        } else {
            sms_center = NULL;
        }
    }
    #endif
    if(sms_center && sms_center[0]) modem_set_sms_center(mdata, sms_center);
    return 0;
}
