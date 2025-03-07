#include "voice.h"
#include "common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

// TODO: Dirty hack. Temporary solution!!!
void lte_voice_incoming_call(int stat, char *number);

// +CLCC: <id1>,<dir>,<stat>,<mode>,<mpty>[,<number>,<type>[,<alpha>][,<priority>][,<CLI validity>]

// Одразу після виконання ATD<n>; приходить:
// +CLCC: 1,0,2,0,0,"+380679332332",145

// Коли пішли гудки (?):
// +CLCC: 1,0,3,0,0,"+380679332332",145

// Коли відповіли на виклик:
// VOICE CALL: BEGIN
// +CLCC: 1,0,0,0,0,"+380679332332",145
// +COLP: "380679332332",161

// Коли поклали слухавку:
// +CLCC: 1,0,6,0,0,"+380679332332",145
// NO CARRIER
// VOICE CALL: END: 000006

// Якшо дали відбій:
// +CLCC: 1,0,6,0,0,"+380679332332",145
// NO CARRIER
// VOICE CALL: END


// При вхідному дзвінку:
// +CLCC: 1,1,4,0,0,"+380679332332",145
// RING

void lte_voice_on_clcc(struct modem_data *mdata, int id, int dir, int stat, char *number)
{
    LOG_INF("lte_voice_on_clcc");
    LOG_INF("id: %d", id);
    LOG_INF("dir: %d", dir);
    LOG_INF("stat: %d", stat);
    LOG_INF("number: %s", number);
    lte_voice_incoming_call(stat, number);
}

int simcom_voice_answer_call(const struct device *dev)
{
    // char send_buf[sizeof("ATA")] = {0};
    // snprintk(send_buf, sizeof(send_buf), "ATA", strlen(topic));
    struct modem_data *mdata = dev->data;
    // struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTSUB: ", on_cmd_subed, 2U, ",");
    return modem_direct_cmd(mdata, "ATA");
}

int simcom_voice_call(const struct device *dev, const char* phone)
{
    char send_buf[sizeof("ATD###############;")] = {0};
    snprintk(send_buf, sizeof(send_buf), "ATD%s;", phone);
    struct modem_data *mdata = dev->data;
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(10));
}


// TODO: move to FileTransmission module (EFS)

// AT+CFTRANRX Transfer a file to EFS
// AT+CFTRANRX=<filepath>,<len>[,<reserved>[,<location>]]
int simcom_efs_download(const struct device *dev, const char *filename, const char *data, size_t data_len)
{
    char send_buf[sizeof("AT+CFTRANRX=\"###########################\",######")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CFTRANRX=\"%s\",%d", filename, data_len);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd_with_direct_payload(mdata, send_buf, data, data_len, NULL, K_MSEC(5000));
}

// AT+CCMXPLAY=<file_name>,<play_path>,<repeat>
// <play_path>
//      0 local path
//      2 remote path (just support voice call)
//      3 local path and remote path
int simcom_voice_play(const struct device *dev, const char *filename, int play_path, int repeat)
{
    char send_buf[sizeof("AT+CCMXPLAY=\"###############################\",#,##")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CCMXPLAY=\"%s\",%d,%d", filename, play_path, repeat);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(10));
    // return modem_cmd(mdata, send_buf, NULL, 0, K_MSEC(5000));
}
