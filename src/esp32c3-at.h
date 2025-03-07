#pragma once

#include <zephyr/kernel.h>
#include "modem_cmd_handler.h"
#include "modem_iface_uart.h"
#include "drivers/at-modem.h"
// #include "id.h"

#define MDM_UART_DEV			  DEVICE_DT_GET(DT_INST_BUS(0))
#define MDM_MAX_DATA_LENGTH		  1*1024
#define MDM_RECV_MAX_BUF		  10
#define MDM_RECV_BUF_SIZE		  1024
#define BUF_ALLOC_TIMEOUT		  K_SECONDS(1)
#define MDM_REGISTRATION_TIMEOUT  K_SECONDS(10)

enum bt_socket_flags {
	ESP_SOCK_IN_USE     = BIT(1),
	ESP_SOCK_CONNECTING = BIT(2),
	ESP_SOCK_CONNECTED  = BIT(3),
	ESP_SOCK_CLOSE_PENDING = BIT(4),
	ESP_SOCK_WORKQ_STOPPED = BIT(5),
};

/* driver data */
struct modem_data {
    /* modem context */
    struct modem_context    mctx;

    /* modem interface */
	struct modem_iface_uart_data iface_data;
    uint8_t iface_rb_buf[MDM_MAX_DATA_LENGTH];

    /* modem cmds */
	struct modem_cmd_handler_data cmd_handler_data;
	uint8_t cmd_match_buf[MDM_RECV_BUF_SIZE + 1];

    /* modem data */
    struct modem_id_data modem_id;

    atmodem_event_cb_t event_cb;

    /* Semaphore(s) */
	struct k_sem sem_response;
    struct k_sem sem_tx_ready;

	/* work */
	struct k_work_q workq;


    /* workers */
    struct k_work_delayable run_worker;
   	struct k_work init_work;

    struct k_fifo run_fifo;
};

struct modem_config {
	const struct gpio_dt_spec bt_on;
};
