#include "wifi.h"

int esp32_wifi_setup(struct modem_data *mdata, const char* sid, const char* password)
{
    char send_buf[sizeof("AT+CWJAP=\"################################################\",\"#############################\"")] = {0};

    esp32_wifi_on(mdata);

    snprintk(send_buf, sizeof(send_buf), "AT+CWJAP=\"%s\",\"%s\"", sid, password);

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

int esp32_wifi_on(struct modem_data *mdata)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+CWMODE=1", &mdata->sem_response, K_SECONDS(2));
}

int esp32_wifi_off(struct modem_data *mdata)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+CWMODE=0", &mdata->sem_response, K_SECONDS(2));
}
