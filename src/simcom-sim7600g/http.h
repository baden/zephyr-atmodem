#pragma once

#include <stdint.h>
#include <stddef.h>
#include <zephyr/toolchain/gcc.h>
#include <zephyr/net/http_parser.h>
#include "../simcom-sim7600g.h"

// TODO: Isolate modem_data from atmodem.h
int simcom_http(struct modem_data *mdata,
    enum http_method method, const char *url,
    const char *content_type,
    const char *accept_type,
    const char *body, size_t data_size,
    struct http_last_action_result *result
);

int simcom_http_read(struct modem_data *mdata, void *response, size_t response_size, size_t *response_data_size);

// enum doit_method {
//     GSM_METHOD_GET=0,
//     GSM_METHOD_POST
// };


int modem_http_init(struct modem_data *mdata);
int modem_http_done(struct modem_data *mdata);
// int modem_http_set_content_type(const char* content_type);
// int modem_http_set_accept_type(const char* content_type);
// int modem_http_set_url(const char* url);
// int modem_http_doit(enum doit_method method);
struct http_last_action_result* modem_http_last_action();
// int simcom_http_read_data(void *buf, size_t len, int flags, size_t *goted_data);

/*
bool gsm_http_doit(int method, int timeout, uint32_t *data_size);
unsigned long gsm_http_server_answer_start(uint32_t data_size);
int gsm_http_url(char *url);
int gsm_http_urlend();

void __printf_like(1, 2) gsmURI(const char *format, ...);

*/
