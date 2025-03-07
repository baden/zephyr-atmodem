#include "id.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

// #include "modem_cmd_handler.h"
// #include "modem_iface_uart.h"

// struct modem_id_data modem_id;

/* Handler: <manufacturer> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_manufacturer)
{
   	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

	size_t out_len = net_buf_linearize(mdata->modem_id.mdm_manufacturer,
					   sizeof(mdata->modem_id.mdm_manufacturer) - 1,
					   data->rx_buf, 0, len);
	mdata->modem_id.mdm_manufacturer[out_len] = '\0';
	// LOG_INF("Manufacturer: %s", /*log_strdup(*/mdata.modem_id.mdm_manufacturer/*)*/);
	return 0;
}

/* Handler: <model> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_model)
{
   	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

	size_t out_len = net_buf_linearize(mdata->modem_id.mdm_model,
					   sizeof(mdata->modem_id.mdm_model) - 1,
					   data->rx_buf, 0, len);
	mdata->modem_id.mdm_model[out_len] = '\0';

	/* Log the received information. */
	// LOG_INF("Model: %s", /*log_strdup(*/mdata.modem_id.mdm_model/*)*/);
	return 0;
}

/* Handler: <rev> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_revision)
{
   	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

	size_t out_len = net_buf_linearize(mdata->modem_id.mdm_revision,
					   sizeof(mdata->modem_id.mdm_revision) - 1,
					   data->rx_buf, 0, len);
	mdata->modem_id.mdm_revision[out_len] = '\0';

    // TODO: Skip over the "+CGMR: " prefix
    // Look quecktel-bg9x.c MODEM_CMD_DEFINE(on_cmd_atcmdinfo_iccid)
    if(mdata->modem_id.mdm_revision[0] == '+') {
        char *p = strchr(mdata->modem_id.mdm_revision, ' ');
        if(p) {
            out_len = strlen(p+1);
            memmove(mdata->modem_id.mdm_revision, p + 1, out_len + 1);
        }
    }

	/* Log the received information. */
	// LOG_INF("Revision: %s", /*log_strdup(*/mdata.modem_id.mdm_revision/*)*/);
	return 0;
}

/* Handler: <IMEI> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_imei)
{
   	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

	size_t out_len = net_buf_linearize(mdata->modem_id.mdm_imei,
					   sizeof(mdata->modem_id.mdm_imei) - 1,
					   data->rx_buf, 0, len);
	mdata->modem_id.mdm_imei[out_len] = '\0';

	/* Log the received information. */
	LOG_INF("LTE IMEI: %s", /*log_strdup(*/mdata->modem_id.mdm_imei/*)*/);
	return 0;
}

// AT+CICCID
// +ICCID: <ICCID>
MODEM_CMD_DEFINE(on_ciccid)
{
   	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    strncpy(mdata->modem_id.mdm_ICCID, argv[0], 32);
    mdata->modem_id.mdm_ICCID[31] = 0;
    return 0;
}


#if defined(CONFIG_MODEM_SIM_NUMBERS)
/* Handler: <IMSI> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_imsi)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
	size_t	out_len = net_buf_linearize(mdata->modem_id.mdm_imsi,
					    sizeof(mdata->modem_id.mdm_imsi) - 1,
					    data->rx_buf, 0, len);
	mdata->modem_id.mdm_imsi[out_len] = '\0';

	/* Log the received information. */
	LOG_INF("IMSI: %s", /*log_strdup(*/mdata->modem_id.mdm_imsi/*)*/);
	return 0;
}
#if 0
/* Handler: <ICCID> */
MODEM_CMD_DEFINE(on_cmd_atcmdinfo_iccid)
{
	size_t out_len;
	char   *p;

	out_len = net_buf_linearize(mdata.mdm_iccid, sizeof(mdata.modem_id.mdm_iccid) - 1,
				    data->rx_buf, 0, len);
	mdata.modem_id.mdm_iccid[out_len] = '\0';

	/* Skip over the +CCID bit, which modems omit. */
	if (mdata.modem_id.mdm_iccid[0] == '+') {
		p = strchr(mdata.modem_id.mdm_iccid, ' ');
		if (p) {
			out_len = strlen(p + 1);
			memmove(mdata.modem_id.mdm_iccid, p + 1, len + 1);
		}
	}

	LOG_INF("ICCID: %s", /*log_strdup(*/mdata.modem_id.mdm_iccid/*)*/);
	return 0;
}
#endif
#endif /* #if defined(CONFIG_MODEM_SIM_NUMBERS) */

// Before SIMcard initialization
static const struct setup_cmd get_id1_cmds[] = {
    SETUP_CMD("AT+CGMI", "", on_cmd_atcmdinfo_manufacturer, 0U, ""),
    SETUP_CMD("AT+CGMM", "", on_cmd_atcmdinfo_model, 0U, ""),
    SETUP_CMD("AT+CGMR", "", on_cmd_atcmdinfo_revision, 0U, ""),
    SETUP_CMD("AT+CGSN", "", on_cmd_atcmdinfo_imei, 0U, ""),
};

// After SIMcard initialization
static const struct setup_cmd get_id2_cmds[] = {
    SETUP_CMD("AT+CICCID", "+ICCID:", on_ciccid, 1U, ""),

	/* Commands to read info from the modem (things like IMEI, Model etc). */
    #if defined(CONFIG_MODEM_SIM_NUMBERS)
    	SETUP_CMD("AT+CIMI", "", on_cmd_atcmdinfo_imsi, 0U, ""),
    	// SETUP_CMD("AT+QCCID", "", on_cmd_atcmdinfo_iccid, 0U, ""),
    #endif /* #if defined(CONFIG_MODEM_SIM_NUMBERS) */
};

// int sim7600g_query_CCID(struct modem_data *mdata)
// {
//     struct modem_cmd cmd = MODEM_CMD("+ICCID:", on_ciccid, 1U, "");
//     return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, "AT+CICCID", &mdata->sem_response, K_SECONDS(1));
// }


int query_ids1(struct modem_data *mdata/*struct modem_context *mctx, struct k_sem *sem_response*/)
{
    /* Run query commands on the modem. */
	return modem_cmd_handler_setup_cmds(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
					   get_id1_cmds, ARRAY_SIZE(get_id1_cmds),
					   &mdata->sem_response, K_SECONDS(5));
}
int query_ids2(struct modem_data *mdata/*struct modem_context *mctx, struct k_sem *sem_response*/)
{
    /* Run query commands on the modem. */
	return modem_cmd_handler_setup_cmds(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
					   get_id2_cmds, ARRAY_SIZE(get_id2_cmds),
					   &mdata->sem_response, K_SECONDS(5));
}

static char *str_unquote(char *str)
{
	char *end;
	if (str[0] != '"') return str;
	str++;
	end = strrchr(str, '"');
	if (end != NULL) *end = 0;
	return str;
}

// TODO: Move to modem_data.
// Example:
// +COPS: 0,0,"UA-KYIVSTAR KYIVSTAR",2
static char opn[32] = "not_detected";
MODEM_CMD_DEFINE(on_cmd_cops)
{
    strncpy(opn, str_unquote(argv[2]), sizeof(opn)-1);
    opn[sizeof(opn)-1] = 0;
    return 0;
}

char* query_operator(struct modem_data *mdata)
{
    struct modem_cmd cmd = MODEM_CMD("+COPS:", on_cmd_cops, 4U, ",");
    if(modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, "AT+COPS?", &mdata->sem_response, K_SECONDS(1)) < 0) {
        strcpy(opn, "unknown");
    }
    return opn;
}
char* operator() { return opn; }

// TODO: Move to API
// char * modem_mdm_manufacturer() { return mdata.modem_id.mdm_manufacturer; }
// char * modem_mdm_model() {return mdata.modem_id.mdm_model; }
// char * modem_mdm_revision() {return mdata.modem_id.mdm_revision; }

// char * modem_mdm_imei() {
//     const struct device *dev = device_get_binding(DT_LABEL(DT_INST(0, simcom_a7682e)));
//     struct modem_data *mdata = dev->data;
//     return mdata->modem_id.mdm_imei;
// }


