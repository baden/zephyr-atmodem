#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include "drivers/at-modem.h"
#include "modem_cmd_handler.h"
#include "modem_iface_uart.h"

#define MDM_UART_DEV			  DEVICE_DT_GET(DT_INST_BUS(0))
#define MDM_MAX_DATA_LENGTH		  (1024)
#define MDM_RECV_MAX_BUF		  6
#define MDM_RECV_BUF_SIZE		  (1024)
#define MDM_REGISTRATION_TIMEOUT  K_SECONDS(10)

enum mqtt_state {
    MQTT_STATE_DISCONNECTED,
    MQTT_STATE_CONNECTING,
    MQTT_STATE_CONNECTED,
    MQTT_STATE_DISCONNECTING,
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
    int mdm_rssi;
	bool cpin_ready;    // If the sim card is ready or not.

	/*
	 * Current state of the modem.
	 */
	enum atmodem_state state;

    enum mqtt_state mqtt_states[2];

    atmodem_event_cb_t event_cb;

    /* Semaphore(s) */
	struct k_sem sem_response;
    struct k_sem sem_tx_ready;

    /* Post OK commands semaphores */
    struct k_sem sem_http_action_response;
    struct k_sem sem_http_file_read;

	/* work */
	struct k_work_q workq;

    /* workers */
    struct k_work_delayable run_worker;

    struct k_fifo run_fifo;
};

struct modem_config {
	const struct gpio_dt_spec status;
	const struct gpio_dt_spec dtr;
	const struct gpio_dt_spec pwrkey;
	const struct gpio_dt_spec power;
};

// extern struct modem_data       mdata;
// extern struct modem_context    mctx;

// TODO: Not best solution. Use mutex, semaphore, or threadsafe bit-field
union _init_stages {
    struct _bits {
        unsigned int rdy:1;
        // unsigned int cpin_rdy:1;
        // OPTIONAL?
        unsigned int sms_done:1;
        unsigned int pb_done:1;
        // Errors
        unsigned int no_sim_card:1;
    } bits;
    uint8_t byte;
};

// TODO: Remove me
int modem_direct_cmd(struct modem_data *mdata, const char* cmd);
