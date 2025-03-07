#include "handlers.h"

#include <stdlib.h>
#include <zephyr/net/buf.h>

#include "tcp.h"
#include "../utils.h"
#include "sms.h"
#include "../at-modem.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_sim7600g, CONFIG_MODEM_LOG_LEVEL);

volatile union _init_stages init_stages;

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

// TODO: move to modem_data.
MODEM_CMD_DEFINE(on_cmd_cpin)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    mdata->cpin_ready = (strcmp(argv[0], "READY") == 0);
    return 0;
}

static const struct modem_cmd unsol_cmds[] = {
	// MODEM_CMD("+QIURC: \"recv\",",	   on_cmd_unsol_recv,  1U, ""),
	// MODEM_CMD("+QIURC: \"closed\",",   on_cmd_unsol_close, 1U, ""),
	MODEM_CMD("RDY", on_cmd_unsol_rdy, 0U, ""),
    MODEM_CMD("+CME ERROR: SIM not inserted", on_cmd_unsol_nosimcard, 0U, ""),
    // MODEM_CMD("+CPIN: READY", on_cmd_unsol_cpinready, 0U, ""),
    MODEM_CMD("SMS DONE", on_cmd_unsol_smsdone, 0U, ""),
    MODEM_CMD("PB DONE", on_cmd_unsol_pbdone, 0U, ""),
    // INCOMING SMS: +CMTI: "SM",<n>
    MODEM_CMD("+CMTI: \"SM\",", on_cmd_cmti, 1U, ""),
    MODEM_CMD("+CMTI: \"ME\",", on_cmd_cmti, 1U, ""),
    MODEM_CMD("+IPCLOSE: ", on_cmd_ipclose, 2U, ","),
    MODEM_CMD("+IPD", on_cmd_ipd, 1U, ""),
    MODEM_CMD("+CPIN: ", on_cmd_cpin, 1U, ""),
};

NET_BUF_POOL_DEFINE(recv_pool, MDM_RECV_MAX_BUF, MDM_RECV_BUF_SIZE, 0, NULL);

int simcom_init_handlers(struct modem_data *mdata)
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
