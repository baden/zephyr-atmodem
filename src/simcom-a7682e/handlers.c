#include "handlers.h"

#include <stdlib.h>
#include <zephyr/net_buf.h>

#include "tcp.h"
#include "../utils.h"
#include "sms.h"
#include "../at-modem.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

volatile union _init_stages init_stages;

#define BUF_ALLOC_TIMEOUT K_SECONDS(1)

/* Handler: OK */
MODEM_CMD_DEFINE(on_cmd_ok)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

    // LOG_DBG("*> got OK");
	modem_cmd_handler_set_error(data, 0);
	k_sem_give(&mdata->sem_response);
	return 0;
}

/* Handler: ERROR */
MODEM_CMD_DEFINE(on_cmd_error)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // LOG_DBG("*> got ERROR");
	modem_cmd_handler_set_error(data, -EIO);
	k_sem_give(&mdata->sem_response);
	return 0;
}

/* Handler: +CME Error: <err>[0] */
MODEM_CMD_DEFINE(on_cmd_exterror)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // LOG_DBG("*> got +CME");
	modem_cmd_handler_set_error(data, -EIO);
	k_sem_give(&mdata->sem_response);
	return 0;
}

MODEM_CMD_DEFINE(on_cmd_unsol_rdy)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // LOG_DBG("on_cmd_unsol_rdy");
    init_stages.bits.rdy = 1;
	/*if(init_stages.byte == 0x07)*/ k_sem_give(&mdata->sem_response);
	return 0;
}

MODEM_CMD_DEFINE(on_cmd_unsol_nosimcard)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // LOG_DBG("on_cmd_unsol_rdy");
    init_stages.bits.no_sim_card = 1;
	/*if(init_stages.byte == 0x0F)*/ k_sem_give(&mdata->sem_response);
	return 0;
}

// MODEM_CMD_DEFINE(on_cmd_unsol_cpinready)
// {
//     LOG_DBG("on_cmd_unsol_cpinready");
//     init_stages.bits.cpin_rdy = 1;
// 	if(init_stages.byte == 0x0F) k_sem_give(&data->sem_response);
//     return 0;
// }

MODEM_CMD_DEFINE(on_cmd_unsol_smsdone)
{
    LOG_DBG("on_cmd_unsol_smsdone");
    init_stages.bits.sms_done = 1;
    /*
	if(init_stages.byte == 0x07) k_sem_give(&data->sem_response);
    */
    return 0;
}

MODEM_CMD_DEFINE(on_cmd_unsol_pbdone)
{
    LOG_DBG("on_cmd_unsol_pbdone");
    init_stages.bits.pb_done = 1;
    /*
	if(init_stages.byte == 0x07) k_sem_give(&data->sem_response);
    */
    return 0;
}

static const struct modem_cmd response_cmds[] = {
	MODEM_CMD("OK", on_cmd_ok, 0U, ""),
	MODEM_CMD("ERROR", on_cmd_error, 0U, ""),
	MODEM_CMD("+CME ERROR: ", on_cmd_exterror, 1U, ""),
};

#if defined(CONFIG_MODEM_SMS)
MODEM_CMD_DEFINE(on_cmd_cmti)
{
    int index = ATOI(argv[0], 0, "CMTI index");
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // LOG_DBG("*> Got +CMTI: %d", index);

    atmodem_run_schedule(&mdata->run_fifo, &mdata->workq, &mdata->run_worker, simcom_sms_read, &index, sizeof(index));
    // LteCheckSMS(index);


    // #if defined(CONFIG_DEBUG_SMS)
    //     LOG_ERR("Remove me");
    //     size_t frags_len = net_buf_frags_len(data->rx_buf);
    //     char _tmp[128+2];
    //     size_t match_len = net_buf_linearize(_tmp, 128, data->rx_buf, 0, frags_len);
    //     _tmp[match_len] = 0;
    //     naviccAddLogSimple(_tmp);
    // #endif
	// data->sock_written = 0;
	// modem_cmd_handler_set_error(data, -EIO);
	// k_sem_give(&data->sem_response);

	return 0;
}
#endif

#if defined(CONFIG_MODEM_TCP)
MODEM_CMD_DEFINE(on_cmd_ipclose)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_tcp_on_close(mdata, atoi(argv[0]), atoi(argv[1]));
    return 0;
}

MODEM_CMD_DEFINE(on_cmd_ipd)
{
	return sockread_common(data, atoi(argv[0]), len);
}
#endif

// TODO: move to modem_data.
MODEM_CMD_DEFINE(on_cmd_cpin)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    mdata->cpin_ready = (strcmp(argv[0], "READY") == 0);
    return 0;
}

//  on_cmd_cgev
MODEM_CMD_DEFINE(on_cmd_cgev)
{
    LOG_DBG("on_cmd_cgev: %s", argv[0]);
    // struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    return 0;
}

// on_cmd_isimaid
MODEM_CMD_DEFINE(on_cmd_isimaid)
{
    LOG_DBG("on_cmd_isimaid");
    return 0;
}

// TODO: move to http module.
// URC Description
// +HTTP_PEER_CLOSED
//      It’s a notification message. While received, it means the
//      connection has been closed by server.
// +HTTP_NONET_EVENT
//      It’s a notification message. While received, it means nowthenetwork is unavailable.

MODEM_CMD_DEFINE(on_cmd_http_peer_closed)
{
    LOG_DBG("on_cmd_http_peer_closed");
    return 0;
}

MODEM_CMD_DEFINE(on_cmd_http_nonet_event)
{
    LOG_DBG("on_cmd_http_nonet_event");
    return 0;
}

#if defined(CONFIG_MODEM_MQTT)

// TODO: move to mqtt module.
// +CMQTTRXSTART: <client_index>,<topic_total_len>,<payload_total_len>
void mqtt_on_rxstart(struct modem_data *mdata, int client_index, int topic_total_len, int payload_total_len);
MODEM_CMD_DEFINE(on_cmd_mqtt_rxstart)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    int client_index = atoi(argv[0]);
    int topic_total_len = atoi(argv[1]);
    int payload_total_len = atoi(argv[2]);
    mqtt_on_rxstart(mdata, client_index, topic_total_len, payload_total_len);
    return 0;
}

// +CMQTTRXEND: <client_index>
void mqtt_on_rxend(struct modem_data *mdata, int client_index);
MODEM_CMD_DEFINE(on_cmd_mqtt_rxend)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    int client_index = atoi(argv[0]);
    mqtt_on_rxend(mdata, client_index);
    return 0;
}

// +CMQTTRXTOPIC: <client_index>,<sub_topic_len>\r\n<sub_topic>
// void mqtt_on_rxtopic(struct modem_data *mdata, int client_index, int sub_topic_len, char *sub_topic);
int mqtt_on_rxtopic_handler(struct modem_cmd_handler_data *data, int client_index, int sub_topic_len, uint16_t len);

MODEM_CMD_DEFINE(on_cmd_mqtt_rxtopic)
{
    // struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    int client_index = atoi(argv[0]);
    int sub_topic_len = atoi(argv[1]);
    return mqtt_on_rxtopic_handler(data, client_index, sub_topic_len, len);
}

// +CMQTTRXPAYLOAD: <client_index>,<sub_payload_len>\r\n<sub_payload>
int mqtt_on_rx_handler(struct modem_cmd_handler_data *data, int client_index, int sub_payload_len, uint16_t len);
MODEM_CMD_DEFINE(on_cmd_mqtt_rxpayload)
{
    // struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    int client_index = atoi(argv[0]);
    int sub_payload_len = atoi(argv[1]);
    return mqtt_on_rx_handler(data, client_index, sub_payload_len, len);
}

// +CMQTTCONNLOST: <client_index>,<cause>
void mqtt_on_connlost(struct modem_data *mdata, int client_index, int cause);
MODEM_CMD_DEFINE(on_cmd_mqtt_connlost)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    int client_index = atoi(argv[0]);
    int cause = atoi(argv[1]);
    mqtt_on_connlost(mdata, client_index, cause);
    return 0;
}

// Втрата з'єднання з сервером MQTT
// +CMQTTCONNLOST: <client_index>,<cause>
// +CMQTTCONNLOST: 0,3
// <cause>:
// The cause of disconnection.
// 1 Socket is closed passively.
// 2 Socket is reset.
// 3 Network is closed.
// void mqtt_on_connlost(struct modem_data *mdata, int client_index, int cause);
// MODEM_CMD_DEFINE(on_cmd_mqtt_connlost)

#endif // CONFIG_MODEM_MQTT

#if defined(CONFIG_MODEM_VOICE)

// TODO: move to voice module.
    // MODEM_CMD("+CLCC: ", on_cmd_clcc, 7U, ","),
    // MODEM_CMD("RING", on_cmd_ring, 0U, ""),

// AT+CLCC List current calls
// +CLCC: <id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>][,<priority>][,<CLI validity>]]
// +CLCC: <id2>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>][,<priority>][,<CLI validity>]]
// <stat> State of the call:
//          0 active
//          1 held
//          2 dialing (MO call)
//          3 alerting (MO call)
//          4 incoming (MT call)
//          5 waiting (MT call)
//          6 disconnect

// Example:
// +CLCC: 1,1,4,0,0,"+380679******",145     - Incoming call
// +CLCC: 1,1,6,0,0,"+380679******",145     - Call ended
void lte_voice_on_clcc(struct modem_data *mdata, int id, int dir, int stat, char *number);
MODEM_CMD_DEFINE(on_cmd_clcc)
{
    LOG_DBG("on_cmd_clcc");
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    #if 1
    int id = atoi(argv[0]);
    int dir = atoi(argv[1]);
    int stat = atoi(argv[2]);
    // char *number = argv[5];
    char number[] = "----";
    lte_voice_on_clcc(mdata, id, dir, stat, number);
    #endif
    // lte_voice_on_clcc(mdata, 1, 1, 4, "---");
    return 0;
}
MODEM_CMD_DEFINE(on_cmd_ring)
{
    LOG_DBG("on_cmd_ring");
    return 0;
}
MODEM_CMD_DEFINE(on_cmd_nocarrier)
{
    LOG_DBG("on_cmd_nocarrier");
    return 0;
}

// Example:
// VOICE CALL: BEGIN        - Система підняля слухавку
// VOICE CALL: END          - Віддалений виклик було відхилено
// VOICE CALL: END: 000010  - Завершення виклику (виклик тривав 10 секунд)
MODEM_CMD_DEFINE(on_cmd_voice_call)
{
    LOG_DBG("on_cmd_voice_call");
    return 0;
}

    // MODEM_CMD("+CCMXPLAY:", on_cmd_voice_play, 0U, ""),
    // MODEM_CMD("+AUDIOSTATE: audio play", on_cmd_voice_audio_play, 0U, ""),
    // MODEM_CMD("+AUDIOSTATE: audio play stop", on_cmd_voice_audio_play_stop, 0U, ""),
MODEM_CMD_DEFINE(on_cmd_voice_play)
{
    LOG_DBG("on_cmd_voice_play");
    return 0;
}
MODEM_CMD_DEFINE(on_cmd_voice_audio_play)
{
    LOG_DBG("on_cmd_voice_audio_play");
    return 0;
}
// TODO: Dirty hack. Temporary solution!!!
void lte_voice_on_play_done();
MODEM_CMD_DEFINE(on_cmd_voice_audio_play_stop)
{
    LOG_DBG("on_cmd_voice_audio_play_stop");
    lte_voice_on_play_done();
    return 0;
}

#endif // CONFIG_MODEM_VOICE

// Варіант відповіді за запи балансу
// +CUSD: 2,"Na Vashomu rahunku 8.05 grn. Taryf 'Vodafone Device M'. Nomer diysnyi do 28.12.2025 00:00:00.", 0

int __attribute__((__weak__)) ussd_on_receive(int n, const char *message)
{
    // LOG_ERR("USSD: %s", message);
    return 0;
}


MODEM_CMD_DEFINE(on_cmd_cusd)
{
    // LOG_DBG("on_cmd_cusd");
    int n = atoi(argv[0]);
    // if(n == 2) {
        // LOG_ERR("USSD: %s", argv[1]);
    // }
    ussd_on_receive(n, argv[1]);
    return 0;
}

static const struct modem_cmd unsol_cmds[] = {
	// MODEM_CMD("+QIURC: \"recv\",",	   on_cmd_unsol_recv,  1U, ""),
	// MODEM_CMD("+QIURC: \"closed\",",   on_cmd_unsol_close, 1U, ""),
	MODEM_CMD("RDY", on_cmd_unsol_rdy, 0U, ""),
    MODEM_CMD("*ATREADY: 1", on_cmd_unsol_rdy, 0U, ""),
    MODEM_CMD("+CME ERROR: SIM not inserted", on_cmd_unsol_nosimcard, 0U, ""),
    // MODEM_CMD("+CPIN: READY", on_cmd_unsol_cpinready, 0U, ""),
    MODEM_CMD("SMS DONE", on_cmd_unsol_smsdone, 0U, ""),
    MODEM_CMD("PB DONE", on_cmd_unsol_pbdone, 0U, ""),
    // INCOMING SMS: +CMTI: "SM",<n>
    #if defined(CONFIG_MODEM_SMS)
    MODEM_CMD("+CMTI: \"SM\",", on_cmd_cmti, 1U, ""),
    MODEM_CMD("+CMTI: \"ME\",", on_cmd_cmti, 1U, ""),
    #endif
    #if defined(CONFIG_MODEM_TCP)
    MODEM_CMD("+IPCLOSE: ", on_cmd_ipclose, 2U, ","),
    MODEM_CMD("+IPD", on_cmd_ipd, 1U, ""),
    #endif
    MODEM_CMD("+CPIN: ", on_cmd_cpin, 1U, ""),
    // +CGEV:
    MODEM_CMD("+CGEV: ", on_cmd_cgev, 1U, ""),
    MODEM_CMD("*ISIMAID: ", on_cmd_isimaid, 1U, ""),

    #if defined(CONFIG_MODEM_MQTT)
    MODEM_CMD("+CMQTTRXSTART: ", on_cmd_mqtt_rxstart, 3U, ","),
    MODEM_CMD("+CMQTTRXEND: ", on_cmd_mqtt_rxend, 1U, ""),
    MODEM_CMD("+CMQTTRXTOPIC: ", on_cmd_mqtt_rxtopic, 2U, ","),
    MODEM_CMD("+CMQTTRXPAYLOAD: ", on_cmd_mqtt_rxpayload, 2U, ","),
    MODEM_CMD("+CMQTTCONNLOST: ", on_cmd_mqtt_connlost, 2U, ","),
    #endif // CONFIG_MODEM_MQTT

// TODO: move to http module.
    MODEM_CMD("+HTTP_PEER_CLOSED", on_cmd_http_peer_closed, 0U, ""),
    MODEM_CMD("+HTTP_NONET_EVENT", on_cmd_http_nonet_event, 0U, ""),

    #if defined(CONFIG_MODEM_VOICE)
// TODO: move to voice module.
    MODEM_CMD("+CLCC: ", on_cmd_clcc, 3U, ","),
    // MODEM_CMD("+CLCC: ", on_cmd_clcc, /1U, ""),
    MODEM_CMD("RING", on_cmd_ring, 0U, ""),
    MODEM_CMD("NO CARRIER", on_cmd_nocarrier, 0U, ""),
    MODEM_CMD("VOICE CALL: ", on_cmd_voice_call, 1U, ""),
    // +CCMXPLAY:
    // +AUDIOSTATE: audio play
    // +AUDIOSTATE: audio play stop
    MODEM_CMD("+CCMXPLAY:", on_cmd_voice_play, 0U, ""),
    MODEM_CMD("+AUDIOSTATE: audio play stop", on_cmd_voice_audio_play_stop, 0U, ""),
    MODEM_CMD("+AUDIOSTATE: audio play", on_cmd_voice_audio_play, 0U, ""),
    #endif // CONFIG_MODEM_VOICE

    MODEM_CMD("+CUSD: ", on_cmd_cusd, 2U, ","),
};

NET_BUF_POOL_DEFINE(recv_pool, MDM_RECV_MAX_BUF, MDM_RECV_BUF_SIZE, 0, NULL);

int simcom_init_handlers(struct modem_data *mdata)
{
    /* Command handler. */
	const struct modem_cmd_handler_config cmd_handler_config = {
        .match_buf          = &mdata->cmd_match_buf[0],
        .match_buf_len      = sizeof(mdata->cmd_match_buf),
        .buf_pool		    = &recv_pool,
        .alloc_timeout	    = BUF_ALLOC_TIMEOUT, //K_NO_WAIT;
        .eol		        = "\r\n",
        .user_data          = NULL,
        .response_cmds      = response_cmds,
        .response_cmds_len  = ARRAY_SIZE(response_cmds),
        .unsol_cmds         = unsol_cmds,
        .unsol_cmds_len     = ARRAY_SIZE(unsol_cmds),
    };
    /* cmd handler */
    // mdata->cmd_handler_data.cmds[CMD_RESP]	    = response_cmds;
	// mdata->cmd_handler_data.cmds_len[CMD_RESP]  = ARRAY_SIZE(response_cmds);
    // mdata->cmd_handler_data.cmds[CMD_UNSOL]	    = unsol_cmds;
	// mdata->cmd_handler_data.cmds_len[CMD_UNSOL] = ARRAY_SIZE(unsol_cmds);
    // mdata->cmd_handler_data.match_buf	        = ;
	// mdata->cmd_handler_data.
	// mdata->cmd_handler_data
	// mdata->cmd_handler_data
	// mdata->cmd_handler_data
	return modem_cmd_handler_init(&mdata->mctx.cmd_handler,
        &mdata->cmd_handler_data, &cmd_handler_config);
}
