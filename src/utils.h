#pragma once

#include<stddef.h>

void hex_to_ascii(char *dest, const char *source, int length);
void ucs2_to_utf8(char *dest, const char *source, int length);
void ucs2_to_ascii(char *dest, const char *source, int length);

/* Modem ATOI routine. */
#define ATOI(s_, value_, desc_)	  atmodem_atoi(s_, value_, desc_, __func__)
// #include<string.h>
/* Func: modem_atoi
* Desc: Convert string to long integer, but handle errors
*/
int atmodem_atoi(const char *s, const int err_value,
		      const char *desc, const char *func);


char *str_unquote(char *str);

// Inspiration:
// https://github.com/nesl/zephyr-wasm/blob/8f51300a84d033ac8e55fdaf8942ff88416342cb/drivers/wifi/esp/esp_offload.c

#define FIND_VAL(res, p,frags_len,parsed_bytes,reg)                 \
    res = find_val(p, frags_len, &parsed_bytes, &reg);              \
    if(res == -2) {                                                 \
        return -EAGAIN;                                             \
    } else if(res == -1) {                                          \
        LOG_ERR("Error parsing " STRINGIFY(reg));                   \
        return len;                                                 \
    }                                                               \
    p += parsed_bytes;


int find_len(char *data);
int find_val(const char* p, size_t data_length, size_t *parsed_bytes, int* v);
int digits(int n);
