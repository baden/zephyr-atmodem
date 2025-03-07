#include "ota.h"

int esp32_ota_start(struct modem_data *mdata, const char* url)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+CIUPDATE", &mdata->sem_response, K_SECONDS(30));
}
