#include "tcp.h"
#include <zephyr/kernel.h>
#include "../utils.h"

#include<stdlib.h>  // atoi

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

// TODO: Do not use global variable! Use driver context.
static char server_ip[16] = "000.000.000.000";

MODEM_CMD_DEFINE(on_cmd_dns_response)
{
    int ret_code = ATOI(argv[0], 0, "ret_code");
    // argv[1] - "name","IP"
    // or
    // error code
    // LOG_ERR("DNS RET:[%d]", ret_code);
    if(ret_code == 1) {
        // argv[2];    // IP
        strncpy(server_ip, str_unquote(argv[2]), sizeof(server_ip)-1);
        server_ip[sizeof(server_ip)-1] = 0;
        LOG_INF("DNS OK [%s]", server_ip);
    } else {
        // TODO: Hardcoded for fail
        strcpy(server_ip, "66.63.177.180"); // TODO: Fix me
        LOG_ERR("DNS FAIL");
    }
    // modem_cmd_handler_set_error(data, 0);   // Both answers is OK
    // k_sem_give(&mdata.sem_response);
    return 0;
}

int simcom_dns(struct modem_data *mdata, const char* domain_name, char* resolved_name)
{
    // AT+CDNSGIP=”www.baidu.com”
    int  ret;
	char send_buf[sizeof("AT+CDNSGIP=\"www.name_can_be_long_but_not_excessive.com\"")] = {0};
    strcpy(resolved_name, "0.0.0.0");

    snprintk(send_buf, sizeof(send_buf), "AT+CDNSGIP=\"%s\"", domain_name);
    // Expect response:
    // +CDNSGIP: 1,<domain name>,<IP address>
    // or
    // +CDNSGIP: 0,<dns error code>

    // Example:
    // +CDNSGIP: 1,"my.delfastbikes.com","66.63.177.180"
    struct modem_cmd cmd = MODEM_CMD_ARGS_MAX("+CDNSGIP:", on_cmd_dns_response, 2U, 3U, ",");
    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, send_buf, &mdata->sem_response, K_SECONDS(30));
    // ret = modem_cmd_send_ext(&mctx.iface, &mctx.cmd_handler, &cmd, 1U, send_buf, &mdata.sem_response, K_SECONDS(2), MODEM_NO_UNSET_CMDS);
    if(ret < 0) {
        LOG_ERR("Error AT+CDNSGIP command");
        goto out;
    }
    /*
    k_sem_reset(&mdata.sem_response);
    // Wait for +CDNSGIP:
    ret = k_sem_take(&mdata.sem_response, K_SECONDS(30));
    if(ret < 0) {
        goto out;
    }
    */
    // TODO.
    // LOG_ERR("TBD");
    strncpy(resolved_name, server_ip, 15); resolved_name[15] = 0;

    out:
        // (void)modem_cmd_handler_update_cmds(&mdata.cmd_handler_data, NULL, 0U, false);

    return ret;
}

// AT+CDNSCFG=<pri_dns>[,<sec_dns>][,<type>]
int simcom_dns_config(const struct device *dev, const char* primary_dns, const char* secondary_dns)
{
    struct modem_data *mdata = dev->data;
    // int ret;

    char send_buf[sizeof("AT+CDNSCFG=###.###.###.###,###.###.###.###")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CDNSCFG=%s,%s", primary_dns, secondary_dns);
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(3));    
}
