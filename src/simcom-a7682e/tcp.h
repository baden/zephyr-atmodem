#pragma once
#if defined(CONFIG_MODEM_TCP)

#include <stdint.h>
#include <stddef.h>
#include <zephyr/toolchain.h>
#include "../simcom-a7682e.h"

int simcom_tcp_context_enable(struct modem_data *mdata);
int simcom_tcp_context_disable(struct modem_data *mdata);
int simcom_tcp_connect(struct modem_data *mdata, int socket_index, const char* ip, uint16_t port);
int simcom_tcp_close(struct modem_data *mdata, int socket_index);
int simcom_tcp_send(struct modem_data *mdata, int socket_index, const uint8_t *buffer, unsigned size);
// TODO: Think about callback only implementation
int simcom_tcp_recv(struct modem_data *mdata, int socket_index, uint8_t *buf, size_t max_len, size_t *readed);


// Private?
int modem_tcp_netopen(struct modem_data *mdata);
int modem_tcp_netclose(struct modem_data *mdata);
int modem_tcp_ipaddr(struct modem_data *mdata);

// Modem private
int modem_tcp_on_close(struct modem_data *data, int client_index, int close_reason);
int sockread_common(struct modem_cmd_handler_data *data, int socket_data_length, uint16_t len);
#endif
