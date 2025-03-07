#include "id.h"

#include <stdlib.h> // strtol

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_esp_at, CONFIG_MODEM_LOG_LEVEL);

#include "modem_cmd_handler.h"

// AT+DFRSAD=1,"WglPeuIrfspRdOCp9AYHjQ==","gKwCAOusPArpXLgPcAhVODyj+kNJtnKvyf7PiFVVW3IYREiqHZpllNgDAA=="
// +DSRSAD: 1,1,+81/thT5TKAwSOQnZ7YB9Ik5h41EaQjOB5aRoYm0sb0=
// +DSRSAD: 1,2,TBD

// TODO: Stupid solution. Use modem context.
volatile char *g_decrypted_SC_b64 = NULL;
volatile size_t g_max_decrypted_SC_b64_length = 0;
extern struct k_sem SC_dectypted;

MODEM_CMD_DEFINE(on_cmd_dsrsad)
{
    // LOG_ERR("***on_cmd_dsrsad");
    if(g_decrypted_SC_b64 == NULL) {
        LOG_ERR("  ***1");
        return 0;
    }
    if(g_max_decrypted_SC_b64_length == 0) {
        LOG_ERR("  ***2");
        return 0;
    }

    int p1 = atoi(argv[0]);
    int p2 = atoi(argv[1]);
    if(p1 != 1) {
        LOG_ERR("  ***3");
        return 0;
    }
    if(p2 == 1) {
        LOG_ERR("Got SC:[%s]", /*log_strdup(*/argv[2]/*)*/);
        strncpy((char *)g_decrypted_SC_b64, argv[2], g_max_decrypted_SC_b64_length);
        g_decrypted_SC_b64[g_max_decrypted_SC_b64_length] = 0;    // Ensure Z-terminating
        k_sem_give(&SC_dectypted);
    } else {
        LOG_ERR("  ***4");
    }

    return 0;
}

// Encode Server Cipher
static int bt_encode_SC(struct modem_data *mdata, char* encrypted_SC_b64, char* decrypted_SC_b64, size_t max_decrypted_SC_b64_length)
{
    struct modem_cmd cmd = MODEM_CMD("+DSRSAD:", on_cmd_dsrsad, 3U, ",");

    LOG_ERR("TODO: Encode Server Cipher (%d)...", max_decrypted_SC_b64_length);

    char send_buf[sizeof("AT+DFRSAD=1,\"WglPeuIrfspRdOCp9AYHjQ==........\",\"gKwCAOusPArpXLgPcAhVODyj+kNJtnKvyf7PiFVVW3IYREiqHZpllNgDAA==............\"")] = {0};
    snprintk(send_buf, sizeof(send_buf),
        "AT+DFRSAD=1,\"WglPeuIrfspRdOCp9AYHjQ==\",\"%s\"", encrypted_SC_b64
    );

    // Bad solution
    g_decrypted_SC_b64 = decrypted_SC_b64;
    g_max_decrypted_SC_b64_length = max_decrypted_SC_b64_length;

    // char send_buf[] = "AT+DFRSAD=1,\"WglPeuIrfspRdOCp9AYHjQ==\",\"gKwCAOusPArpXLgPcAhVODyj+kNJtnKvyf7PiFVVW3IYREiqHZpllNgDAA==\"";

    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, send_buf, &mdata->sem_response, K_SECONDS(1));
}

int esp32_encode_SC(const struct device *dev, char* encrypted_SC_b64, char* decrypted_SC_b64, size_t max_decrypted_SC_b64_length)
{
    struct modem_data *mdata = dev->data;
    return bt_encode_SC(mdata, encrypted_SC_b64, decrypted_SC_b64, max_decrypted_SC_b64_length);
}
