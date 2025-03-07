#include "ussd.h"
#include "common.h"
#include "sms.h"
#include "drivers/at-modem.h"


// AT+CUSD=1,\"code\"
int simcom_ussd_request(const struct device *dev, const char *code)
{
    struct modem_data *mdata = dev->data;

    char send_buf[sizeof("AT+CUSD=1,\"###########################\"")] = {0};

    // modem_char_set(mdata, GSM_CHARSET_GSM, SMS_FLAGS_DEFAULT);
   
    snprintk(send_buf, sizeof(send_buf), "AT+CUSD=1,\"%s\",15", code);
    return modem_direct_cmd(mdata, send_buf);
}

