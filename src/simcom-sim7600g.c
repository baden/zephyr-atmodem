#include "simcom-sim7600g.h"

#define DT_DRV_COMPAT simcom_sim7600g

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(modem_sim7600g, CONFIG_MODEM_LOG_LEVEL);

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "drivers/at-modem.h"
#include "at-modem.h"

#include "utils.h"
#include "simcom-sim7600g/onoff.h"
#include "simcom-sim7600g/id.h"
#include "simcom-sim7600g/http.h"
#include "simcom-sim7600g/gprs.h"
#include "simcom-sim7600g/tcp.h"
#include "simcom-sim7600g/sms.h"
#include "simcom-sim7600g/dns.h"
#include "simcom-sim7600g/handlers.h"

static struct k_thread	       modem_rx_thread;
static struct modem_data       mdata;

static K_KERNEL_STACK_DEFINE(modem_rx_stack, CONFIG_MODEM_SIM7600G_RX_STACK_SIZE);

static const struct modem_config modem_driver_config = {
    .status    = GPIO_DT_SPEC_INST_GET(0,    status_gpios),
    .dtr       = GPIO_DT_SPEC_INST_GET(0,       dtr_gpios),
    .pwrkey    = GPIO_DT_SPEC_INST_GET(0,    pwrkey_gpios),
    .power     = GPIO_DT_SPEC_INST_GET(0,     power_gpios),
    .discharge = GPIO_DT_SPEC_INST_GET(0, discharge_gpios),
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

static K_KERNEL_STACK_DEFINE(modem_workq_stack, CONFIG_MODEM_SIM7600G_WORKQ_STACK_SIZE);
static void modem_run_work(struct k_work *work);

static int init(const struct device *dev)
{
    int ret = 0;
	struct modem_data *mdata = dev->data;
    mdata->dev = dev;

    struct device *uart_dev = (struct device *)MDM_UART_DEV;
	LOG_INF("SIMCOM SIM7600G LTE-modem (@UART:%s)", uart_dev->name);

    mdata->state = ATMODEM_STATE_OFF;

    k_sem_init(&mdata->sem_response,	 0, 1);
    k_sem_init(&mdata->sem_tx_ready,	 0, 1);

	/* initialize the work queue */
	k_work_queue_start(&mdata->workq, modem_workq_stack,
			   K_KERNEL_STACK_SIZEOF(modem_workq_stack),
			   K_PRIO_COOP(CONFIG_MODEM_SIM7600G_WORKQ_THREAD_PRIORITY),
			   NULL);
	k_thread_name_set(&mdata->workq.thread, "SIMCOM:WQ");

    /* Workers */
    k_work_init_delayable(&mdata->run_worker, modem_run_work);
    k_fifo_init(&mdata->run_fifo);

    ret = simcom_init_handlers(mdata);
	if (ret < 0) {
		goto error;
	}

    /* modem interface */
    // mdata->iface_data.hw_flow_control = DT_PROP(MODEM_BUS, hw_flow_control);
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
	k_thread_create(&modem_rx_thread, modem_rx_stack,
			K_KERNEL_STACK_SIZEOF(modem_rx_stack),
			(k_thread_entry_t)modem_rx,
			mdata, NULL, NULL,
            K_PRIO_COOP(7), 0, K_NO_WAIT);
    k_thread_name_set(&modem_rx_thread, "SIMCOM:RX");

	// mdata->net_iface = NET_IF_GET(Z_DEVICE_DT_DEV_NAME(DT_DRV_INST(0)), 0);

    error:
    	return ret;
}

int modem_direct_cmd(struct modem_data *mdata, const char* cmd)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, cmd, &mdata->sem_response, K_SECONDS(2));
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

enum atmodem_state get_state(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return mdata->state;
}

static int set_event_cb(const struct device *dev, atmodem_event_cb_t cb)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    mdata->event_cb = cb;
    return 0;
}

static int http(const struct device *dev,
    enum http_method method, const char *url,
    const char *content_type,
    const char *accept_type,
    const char *body, size_t data_size,
    struct http_last_action_result *result
)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_http(mdata,
        method, url,
        content_type,
        accept_type,
        body, data_size,
        result
    );
}

static int http_read(const struct device *dev, char *response, size_t response_size, size_t *response_data_size)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_http_read(mdata, response, response_size, response_data_size);
}

static int dns(const struct device *dev, const char* domain_name, char* resolved_name)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_dns(mdata, domain_name, resolved_name);
}

static int tcp_connect(const struct device *dev, int socket_index, const char* host, uint16_t port)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_tcp_connect(mdata, socket_index, host, port);
}

static int tcp_send(const struct device *dev, int socket_index, const char* data, size_t data_size)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_tcp_send(mdata, socket_index, data, data_size);
}

static int tcp_recv(const struct device *dev, int socket_index, char* data, size_t data_size, size_t *recv_size)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_tcp_recv(mdata, socket_index, data, data_size, recv_size);
}

static int tcp_close(const struct device *dev, int socket_index)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_tcp_close(mdata, socket_index);
}

static int tcp_context_enable(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_tcp_context_enable(mdata);
}

static int tcp_context_disable(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_tcp_context_disable(mdata);
}

static struct modem_id_data *get_id(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return &mdata->modem_id;
}

static int send_sms(const struct device *dev, const char *phone, const char *message)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_send_sms(mdata, phone, message);
}

static int setup(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    const struct modem_config *config = dev->config;
    return simcom_setup(mdata, config);
}

static int stop(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    const struct modem_config *config = dev->config;
    return simcom_stop(mdata, config);
}

static int __handler_result_parser(const struct device *dev, int ret)
{
    // struct modem_data *mdata = (struct modem_data *)dev->data;
    static unsigned lte_errors = 0;
    // LOG_ERR("at_modem_fifo_handler ret: %d", ret);

    if(ret < 0) {

        LOG_ERR("TODO: SIMCOM handler failed: %d", ret);
        // Fast LTE stack reinit
        lte_errors++;

        if(lte_errors >= 5) {
            lte_errors = 0;
            // struct modem_data *data = (struct modem_data *)dev->data;
            // if(data->event_cb) {
            //     data->event_cb(ATMODEM_EVENT_ERROR, NULL, 0);
            // }

            if(modem_full_reinit(dev) < 0) {
                LOG_ERR("Error LTE reinit. Hm. What to do?");
            }
        } else {
            if(modem_fast_reinit(dev) < 0) {
                LOG_ERR("Error fast reinit. Need slow reinit?");
            } else {
                // TODO: Think about it. Maybe retry handler?
            }
        }
    } else {
        lte_errors = 0;
    }

    return 0;
}

static struct atmodem_driver_api api_funcs = {
	.setup = setup,
	.stop = stop,
    .run = run,
    .work = work,
    .set_event_cb = set_event_cb,
    .get_state = get_state,
    .get_id = get_id,

    // High level API
    .http = http,
    .http_read = http_read,
    .dns = dns,
    .tcp_connect = tcp_connect,
    .tcp_send = tcp_send,
    .tcp_recv = tcp_recv,
    .tcp_close = tcp_close,
    .send_sms = send_sms,

    // TODO: Deprecated API
    .tcp_context_enable = tcp_context_enable,
    .tcp_context_disable = tcp_context_disable,

    // Internally used. Do not call outside
    .__handler_result_parser = __handler_result_parser,
};

// DEVICE_DT_INST_DEFINE
// DEVICE_DT_DEFINE(DT_INST(0, simcom_sim7600g),
DEVICE_DT_INST_DEFINE(0,
    init, NULL, &mdata, &modem_driver_config,
	POST_KERNEL, 42/*CONFIG_MODEM_GSM_INIT_PRIORITY*/, &api_funcs
);


/* Register device with the networking stack. */
// NET_DEVICE_DT_INST_OFFLOAD_DEFINE(0, modem_init, NULL, &mdata, NULL,
// 				  CONFIG_MODEM_SIMCOM_INIT_PRIORITY, &api_funcs,
// 				  MDM_MAX_DATA_LENGTH);

// NET_SOCKET_OFFLOAD_REGISTER(simcom_sim7080, CONFIG_NET_SOCKETS_OFFLOAD_PRIORITY,
// 			    AF_UNSPEC, offload_is_supported, offload_socket);

