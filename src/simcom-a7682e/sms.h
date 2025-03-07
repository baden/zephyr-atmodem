#pragma once

#include "../simcom-a7682e.h"

// #include <kernel.h>
// #include "modem_context.h"


typedef enum {
    GSM_CHARSET_GSM = 0,    //GSM 7 bit default alphabet (3GPP TS 23.038);
    GSM_CHARSET_IRA,
    /*
        16-bit universal multiple-octet coded character set
        (ISO/IEC10646); UCS2 character strings are
        converted to hexadecimal numbers from 0000 to
        FFFF; e.g. "004100620063" equals three 16-bit
        characters with decimal values 65, 98 and 99
    */
    GSM_CHARSET_UCS2
} t_sms_charset;

typedef enum {
    SMS_FLAGS_DEFAULT = 0,
    SMS_FLAGS_FLASH = 1,        // Использовать FLASH-SMS
} t_sms_flags;


int simcom_send_sms(struct modem_data *mdata, const char *payload, const char *phone);
int modem_send_cmd(struct modem_data *mdata, const char *command, const char *sender);
int simcom_sms_read(void *payload, size_t payload_size);
int modem_char_set(struct modem_data *mdata, t_sms_charset charset, t_sms_flags flags);

// int modem_char_set(t_sms_charset charset, t_sms_flags flags);
// int modem_check_sms(int index);
// int modem_delete_sms(int index);

// void sim7600_sms_read(struct modem_data *mdata, int index);

int simcom_setup_sms_center(struct modem_data *mdata);
