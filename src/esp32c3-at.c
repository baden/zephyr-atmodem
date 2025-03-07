#include "esp32c3-at.h"

#define DT_DRV_COMPAT espressif_esp32c3at

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(modem_esp_at, CONFIG_MODEM_LOG_LEVEL);

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "drivers/at-modem.h"
#include "at-modem.h"

#include "utils.h"
#include "esp32c3-at/onoff.h"
#include "esp32c3-at/handlers.h"
#include "esp32c3-at/wifi.h"
#include "esp32c3-at/ble/at.h"
#include "esp32c3-at/ble/server.h"
#include "esp32c3-at/ble/id.h"
#include "esp32c3-at/ota.h"
#include "esp32c3-at/ble.h"
#include "ble/write.h"   // TODO: WTF? Why is this here?

static struct k_thread	       modem_rx_thread;
static struct modem_data       mdata;

static K_KERNEL_STACK_DEFINE(bt_modem_rx_stack, CONFIG_MODEM_ESP32_AT_RX_STACK_SIZE);

static const struct modem_config modem_driver_config = {
    .bt_on = GPIO_DT_SPEC_INST_GET(0, bt_on_gpios)
};

/* Func: modem_rx
 * Desc: Thread to process all messages received from the Modem.
 */
static void modem_rx(void *p1, void *p2, void *p3)
{
    struct modem_data *mdata = (struct modem_data *)p1;

	while (true) {
		/* Wait for incoming data */
		k_sem_take(&mdata->iface_data.rx_sem, K_FOREVER);
        // LOG_DBG("modem_rx incoming data");
		mdata->mctx.cmd_handler.process(&mdata->mctx.cmd_handler, &mdata->mctx.iface);
        /* give up time if we have a solid stream of data */
        k_yield();
	}
}

static K_KERNEL_STACK_DEFINE(modem_workq_stack, CONFIG_MODEM_ESP32_AT_WORKQ_STACK_SIZE);
static void modem_run_work(struct k_work *work);

static void init_work(struct k_work *work)
{
    struct modem_data *mdata = CONTAINER_OF(work, struct modem_data, init_work);
    int ret = esp32_init(mdata);
    if(ret < 0) {
        LOG_ERR("esp32_init failed: %d", ret);
        return;
    }
}


static int init(const struct device *dev)
{
    int ret = 0;
	struct modem_data *mdata = dev->data;

    struct device *uart_dev = (struct device *)MDM_UART_DEV;
	LOG_INF("ESP32C3 BT/WIFI-modem (@UART:%s)", uart_dev->name);

    k_sem_init(&mdata->sem_response,	 0, 1);
    k_sem_init(&mdata->sem_tx_ready,	 0, 1);

	/* initialize the work queue */
	k_work_queue_start(&mdata->workq, modem_workq_stack,
			   K_KERNEL_STACK_SIZEOF(modem_workq_stack),
			   K_PRIO_COOP(CONFIG_MODEM_SIM7600G_WORKQ_THREAD_PRIORITY),
			   NULL);
	k_thread_name_set(&mdata->workq.thread, "ESP32:WQ");

    /* Workers */
    k_work_init_delayable(&mdata->run_worker, modem_run_work);
    k_fifo_init(&mdata->run_fifo);
    k_work_init(&mdata->init_work, init_work);

    ret = esp32at_init_handlers(mdata);
	if (ret < 0) {
		goto error;
	}

    /* modem interface */
	mdata->iface_data.rx_rb_buf     = &mdata->iface_rb_buf[0];
	mdata->iface_data.rx_rb_buf_len = sizeof(mdata->iface_rb_buf);
	ret = modem_iface_uart_init(&mdata->mctx.iface, &mdata->iface_data, MDM_UART_DEV);
	if (ret < 0) {
		goto error;
	}

    /* pin setup */
	mdata->mctx.driver_data = mdata;

    ret = modem_context_register(&mdata->mctx);
	if (ret < 0) {
		LOG_ERR("Error registering modem context: %d", ret);
		goto error;
	}

    /* start RX thread */
	k_thread_create(&modem_rx_thread, bt_modem_rx_stack,
			K_KERNEL_STACK_SIZEOF(bt_modem_rx_stack),
			(k_thread_entry_t)modem_rx,
			mdata, NULL, NULL,
            K_PRIO_COOP(7), 0, K_NO_WAIT);

    k_thread_name_set(&modem_rx_thread, "ESP32:RX");

    error:
    	return ret;
}


// APIs

static int setup(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    const struct modem_config *config = dev->config;
    return esp32_setup(mdata, config);
}

static int stop(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    const struct modem_config *config = dev->config;
    return esp32_stop(mdata, config);
}

static void modem_run_work(struct k_work *work)
{
    // TODO: I don't like this, but it's the only way to get the modem device reference
    const struct device *dev = DEVICE_DT_GET(DT_DRV_INST(0));

    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct modem_data *mdata = CONTAINER_OF(delayable, struct modem_data, run_worker);
    struct k_fifo *run_fifo = &mdata->run_fifo;
    struct k_work_q *workq = &mdata->workq;

    atmodem_execute(dev, run_fifo, workq, delayable);
}

static int run(const struct device *dev, atmodem_work_handler_t handler, void *payload, size_t payload_size)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return atmodem_run_schedule(&mdata->run_fifo, &mdata->workq, &mdata->run_worker, handler, payload, payload_size);
}

// Run on modem work queue
static int work(const struct device *dev, struct k_work_delayable *worker)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    // k_work_submit_to_queue(&data->workq, worker);
    atmodem_worker_schedule(&mdata->workq, worker);
    return 0;
}


static int set_event_cb(const struct device *dev, atmodem_event_cb_t cb)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    mdata->event_cb = cb;
    return 0;
}

static struct modem_id_data *get_id(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return &mdata->modem_id;
}

static int wifi_setup(const struct device *dev, const char* sid, const char* password)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_wifi_setup(mdata, sid, password);
}

static int wifi_on(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_wifi_on(mdata);
}

static int wifi_off(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_wifi_off(mdata);
}

static int ble_start_server(const struct device *dev, const char* name)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_ble_start_server(mdata, name);
}

static int ble_start_adv(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_ble_start_adv(mdata);
}

static int ble_notify(const struct device *dev, unsigned conn_index, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_ble_notify(mdata, conn_index, srv_index, char_index, length, buffer);
}

static int ble_notify_subscribers(const struct device *dev, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_ble_notify_subscribers(mdata, srv_index, char_index, length, buffer);
}

static int ble_set_attr(const struct device *dev, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_ble_set_attr(mdata, srv_index, char_index, length, buffer);
}

// OTA
static int ota_start(const struct device *dev, const char* url)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_ota_start(mdata, url);
}

static int __handler_result_parser(const struct device *dev, int ret)
{
    if(ret < 0) {
        LOG_ERR("ESP32 Error: %d. TODO: Restart?", ret);
        return ret;
    }
    return 0;
}

static struct atmodem_driver_api api_funcs = {
	.setup = setup,
	.stop = stop,
    .run = run,
    .work = work,
    .set_event_cb = set_event_cb,
    .get_state = NULL,
    .get_id = get_id,

    // High level API
    .http = NULL,
    .http_read = NULL,
    .dns = NULL,
    .tcp_connect = NULL,
    .tcp_send = NULL,
    .tcp_recv = NULL,
    .tcp_close = NULL,

    .send_sms = NULL,

    .wifi_setup = wifi_setup,
    .wifi_on = wifi_on,
    .wifi_off = wifi_off,

    .ble_start_server = ble_start_server,
    .ble_start_adv = ble_start_adv,
    .ble_notify = ble_notify,
    .ble_notify_subscribers = ble_notify_subscribers,
    .ble_set_attr = ble_set_attr,

    // OTA
    .ota_start = ota_start,

    // TODO: Deprecated API
    .tcp_context_enable = NULL,
    .tcp_context_disable = NULL,

    // Internally used. Do not call outside
    .__handler_result_parser = __handler_result_parser,
};

DEVICE_DT_INST_DEFINE(0,
    init, NULL, &mdata, &modem_driver_config,
	POST_KERNEL, 42/*CONFIG_MODEM_GSM_INIT_PRIORITY*/, &api_funcs);
