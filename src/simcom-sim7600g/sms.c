#include "sms.h"
#include "lte/lte.h"
#include "ucs2.h"
#include "drivers/at-modem.h"
#include "nvm/nvm.h"
#include "id.h"
#include "../utils.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_sim7600g, CONFIG_MODEM_LOG_LEVEL);

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
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+CMGF=1", &mdata->sem_response, K_SECONDS(2));
}

static int modem_send_sms_select_message_service0(struct modem_data *mdata)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+CSMS=0", &mdata->sem_response, K_SECONDS(2));
}

// static int modem_send_sms_select_message_service1()
// {
//     return modem_cmd_send(&mctx.iface, &mctx.cmd_handler, NULL, 0U, "AT+CSMS=1", &mdata.sem_response, K_SECONDS(2));
// }

static int modem_send_sms_preffered_message_storage_me(struct modem_data *mdata)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+CPMS=\"ME\",\"ME\",\"ME\"", &mdata->sem_response, K_SECONDS(2));
}

static int modem_char_set(struct modem_data *mdata, t_sms_charset charset, t_sms_flags flags)
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

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

int simcom_send_sms(struct modem_data *mdata, const char *message, const char *phone)
{
    int  ret;
	char send_buf[sizeof("AT+CMGS=\"###############\"")] = {0};
	char ctrlz = 0x1A;

    modem_send_sms_stages = 1;
    modem_send_sms_select_message_service0(mdata);
    modem_send_sms_preffered_message_storage_me(mdata);
    modem_send_sms_text_format(mdata);
    // TODO: For now, only 7bit charset supported

    // TODO: Restore me
    // #warning Restore me
    modem_char_set(mdata, GSM_CHARSET_GSM, SMS_FLAGS_DEFAULT);

	// if (buf_len > MDM_MAX_DATA_LENGTH) {
	// 	buf_len = MDM_MAX_DATA_LENGTH;
	// }

	/* Create a buffer with the correct params. */
	// mdata.sock_written = buf_len;
	snprintk(send_buf, sizeof(send_buf), "AT+CMGS=\"%s\"", phone);

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
	mdata->mctx.iface.write(&mdata->mctx.iface, message, strlen(message));
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

static int intlength(int v)
{
    if(v >= 1000) return 4;
    if(v >= 100) return 3;
    if(v >= 10) return 2;
    return 1;
}

/* Handler: +CMGR: ... hard to parse */
// Fake delim on timetamp 21/11/26,09:12:02+08
// +CMGR: "REC READ","7512110511811511697114","","21/11/26,09:12:02+08",208,96,0,8,"+380672020020",145,67
//        0          1                        2  3         4            5   6  7 8 9               10  11

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
MODEM_CMD_DEFINE(on_cmd_cmgr)
{
    int ret;
    struct sms_payload sms;
    char body[128*4 + 1];

    //sms_body_inUCS2 = ATOI(argv[8], 0, "ucs2") == 8;
    size_t sms_body_length = ATOI(argv[11], 0, "length");

    int skips = intlength(sms_body_length) + 2; // length value + 0x0A + 0x0D

    if (net_buf_frags_len(data->rx_buf) < (sms_body_length*4 + skips)) {
		LOG_DBG("Not enough data -- wait!");
		return -EAGAIN;
	}

    ucs2_to_ascii(sms.phone, argv[1], sizeof(sms.phone));
    LOG_ERR("Sender: [%s]", sms.phone);

    // Skip length value and 0x0A, 0x0D
    data->rx_buf = net_buf_skip(data->rx_buf, skips);

    ret = net_buf_linearize(body, sizeof(body)-1, data->rx_buf, 0, (uint16_t)sms_body_length*4);
    if (ret != sms_body_length*4) {
		LOG_ERR("Total copied data is different then received data!"
			" copied:%d vs. received:%d",
			ret, sms_body_length*4);
		goto exit;
		ret = -EINVAL;
	}

    data->rx_buf = net_buf_skip(data->rx_buf, ret+2);   // Skip last 0x0A, 0x0D ?

    size_t tlen = MIN(sms_body_length+1, sizeof(sms.message));
    ucs2_to_ascii(sms.message, body, tlen);

    LOG_HEXDUMP_ERR(sms.message, sms_body_length+1, "SMS body");

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
        MODEM_CMD("+CMGR: ", on_cmd_cmgr, 12U, ",")
    };
    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, cmds, ARRAY_SIZE(cmds), send_buf, &mdata->sem_response, K_SECONDS(2));
    if (ret < 0) {
        LOG_ERR("Error reading SMS: (%d)", ret);
        // naviccAddLogSimple("Error reading SMS");
        goto exit;
    }

    //#if defined(CONFIG_DEBUG_SMS)
    // naviccAddLogSimple("Got SMS: %s from %s", sms_body, sms_sender);
    //#endif
exit:
    return ret;
}

static int modem_delete_sms(struct modem_data *mdata, int index)
{
    int  ret;
	char send_buf[sizeof("AT+CMGD=##,##")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMGD=%d", index);
    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
    if (ret < 0) {
        LOG_ERR("Error deleting SMS: (%d)", ret);
        return ret;
    }
    return 0;
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
    const char *sms_center = "";
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
    if(sms_center) modem_set_sms_center(mdata, sms_center);
    return 0;
}
