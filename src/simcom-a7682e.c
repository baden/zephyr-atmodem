#include "simcom-a7682e.h"

#define DT_DRV_COMPAT simcom_a7682e

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(modem_a7682e, CONFIG_MODEM_LOG_LEVEL);

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "drivers/at-modem.h"
#include "at-modem.h"

#include "utils.h"
#include "simcom-a7682e/onoff.h"
#include "simcom-a7682e/id.h"
#include "simcom-a7682e/http.h"
#include "simcom-a7682e/gprs.h"
#include "simcom-a7682e/tcp.h"
#include "simcom-a7682e/sms.h"
#include "simcom-a7682e/dns.h"
#include "simcom-a7682e/handlers.h"

static struct k_thread	       modem_rx_thread;
static struct modem_data       mdata;

static K_KERNEL_STACK_DEFINE(modem_rx_stack, CONFIG_MODEM_A7682E_RX_STACK_SIZE);

static const struct modem_config modem_driver_config = {
    .status    = GPIO_DT_SPEC_INST_GET(0,    status_gpios),
    .dtr       = GPIO_DT_SPEC_INST_GET(0,       dtr_gpios),
    .pwrkey    = GPIO_DT_SPEC_INST_GET(0,    pwrkey_gpios),
    .power     = GPIO_DT_SPEC_INST_GET(0,     power_gpios),
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

static K_KERNEL_STACK_DEFINE(modem_workq_stack, CONFIG_MODEM_A7682E_WORKQ_STACK_SIZE);
static void modem_run_work(struct k_work *work);

static int init(const struct device *dev)
{
    int ret = 0;
	struct modem_data *mdata = dev->data;
    mdata->dev = dev;

    // struct device *uart_dev = (struct device *)MDM_UART_DEV;
	// LOG_INF("SIMCOM SIM7600G LTE-modem (@UART:%s)", uart_dev->name);

    mdata->state = ATMODEM_STATE_OFF;

    k_sem_init(&mdata->sem_response,	 0, 1);
    k_sem_init(&mdata->sem_response2,	 0, 1);
    k_sem_init(&mdata->sem_tx_ready,	 0, 1);
    k_sem_init(&mdata->sem_http_action_response, 0, 1);
    k_sem_init(&mdata->sem_http_file_read, 0, 1);

	/* initialize the work queue */
	k_work_queue_start(&mdata->workq, modem_workq_stack,
			   K_KERNEL_STACK_SIZEOF(modem_workq_stack),
			   K_PRIO_COOP(7),
			   NULL);
	k_thread_name_set(&mdata->workq.thread, "SIMCOM:WQ");

	/* Assume the modem is not registered to the network. */
	// mdata->mdm_registration = 0;
	// mdata->cpin_ready = false;
	// mdata->pdp_active = false;
	// mdata.sms_buffer = NULL;
	// mdata.sms_buffer_pos = 0;

    /* Workers */
    k_work_init_delayable(&mdata->run_worker, modem_run_work);
    k_fifo_init(&mdata->run_fifo);

    ret = simcom_init_handlers(mdata);
	if (ret < 0) {
		goto error;
	}

    /* modem interface */
    // // mdata->iface_data.hw_flow_control = DT_PROP(MODEM_BUS, hw_flow_control);
	// mdata->iface_data.rx_rb_buf     = &mdata->iface_rb_buf[0];
	// mdata->iface_data.rx_rb_buf_len = sizeof(mdata->iface_rb_buf);

	/* Uart handler. */
	const struct modem_iface_uart_config uart_config = {
		.rx_rb_buf = &mdata->iface_rb_buf[0],
		.rx_rb_buf_len = sizeof(mdata->iface_rb_buf),
		.dev = MDM_UART_DEV,
		// .hw_flow_control = DT_PROP(MDM_UART_NODE, hw_flow_control),
	};

	ret = modem_iface_uart_init(&mdata->mctx.iface, &mdata->iface_data, &uart_config);
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

// int modem_direct_cmd(struct modem_data *mdata, const char* cmd)
// {
//     return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, cmd, &mdata->sem_response, K_SECONDS(2));
// }

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

#if defined(CONFIG_MODEM_HTTP)
static int http(const struct device *dev,
    const struct http_config *config,
    struct http_last_action_result *result,
    const char *url
)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_http(mdata, config, result, url);
}

static int http_read(const struct device *dev, char *response, size_t response_size, size_t *response_data_size)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_http_read(mdata, response, response_size, response_data_size);
}
#endif

#if defined(CONFIG_MODEM_DNS)
static int dns(const struct device *dev, const char* domain_name, char* resolved_name)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_dns(mdata, domain_name, resolved_name);
}
#endif

#if defined(CONFIG_MODEM_TCP)
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
#endif

static struct modem_id_data *get_id(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return &mdata->modem_id;
}

#if defined(CONFIG_MODEM_SMS)
static int send_sms(const struct device *dev, const char *phone, const char *message)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_send_sms(mdata, phone, message);
}
#endif

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

    #if defined(CONFIG_MODEM_HTTP)
    // High level API
    .http = http,
    .http_read = http_read,
    #endif
    #if defined(CONFIG_MODEM_DNS)
    .dns = dns,
    #endif
    #if defined(CONFIG_MODEM_TCP)
    .tcp_connect = tcp_connect,
    .tcp_send = tcp_send,
    .tcp_recv = tcp_recv,
    .tcp_close = tcp_close,
    #endif

    #if defined(CONFIG_MODEM_SMS)
    .send_sms = send_sms,
    #endif

    #if defined(CONFIG_MODEM_TCP)
    // TODO: Deprecated API
    .tcp_context_enable = tcp_context_enable,
    .tcp_context_disable = tcp_context_disable,
    #endif

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




// #include <zephyr/kernel.h>
// #include "common.h"

// #include <zephyr/logging/log.h>
// LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

static bool simcom_STATUS_pin_is_present(struct modem_data *mdata)
{
    // TODO: Можливо треба прибрати пряме визначення на dev в mdata
    // Get struct modem_config for my device
    // 1st: i must get pointer to dev struct
    // 2nd: i must get pointer to modem_config struct
    // LOG_ERR("How i can get config?");
    // dev->data is a pointer of dev->data
    // const struct device *dev = CONTAINER_OF(mdata, struct modem_data, mctx);
    const struct device *dev = mdata->dev;
    const struct modem_config *config = dev->config;

    // LOG_ERR("Pointer of &config->status: %p [%d]", &config->status, gpio_pin_get_dt(&config->status));
    // Check STATUS pin
    bool is_status_present = (gpio_pin_get_dt(&config->status) == 1);
    if(!is_status_present) {
        LOG_ERR("==== STATUS pin is LOW ====");
    }
    return is_status_present;
}

// Commands with simple wait answer
int simcom_cmd_with_simple_wait_answer(
    struct modem_data *mdata,
    const char *send_buf,
    const struct modem_cmd handler_cmds[],
    size_t handler_cmds_len,
    k_timeout_t timeout
)
{
    if(!simcom_STATUS_pin_is_present(mdata)) return -EIO;

    LOG_ERR("DEBUG CMD: [%s].", send_buf);

    k_sem_reset(&mdata->sem_response2); // TODO: Не впевнен шо це ідеальне рішення.
    // Можливо таки треба сбрасувати після отримання sem_response?

    /* Send the Modem command. */
    int ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
        handler_cmds, handler_cmds_len,
        send_buf, &mdata->sem_response, timeout/*K_SECONDS(2)*/, MODEM_NO_UNSET_CMDS
    );

    if (ret < 0) {
        LOG_ERR("Failed to send command [%s]. No response OK", send_buf);
        goto exit;
    }
    // k_sem_reset(&mdata->sem_response);

    /* Wait for response2  */
    ret = k_sem_take(&mdata->sem_response2, timeout);
    if (ret < 0) {
        LOG_ERR("No got handler response from modem");
        goto exit;
    }
    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
    if (ret < 0) {
        LOG_ERR("Got error: %d", ret);
        goto exit;
    }

    // TODO: Maybe need callback here?

exit:
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data, NULL, 0U, false);
    return ret;
}

/* Handler for direct commands, expected data after ">" symbol */
MODEM_CMD_DIRECT_DEFINE(on_cmd_tx_ready)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
	k_sem_give(&mdata->sem_tx_ready);
    // LOG_ERR("on_cmd_tx_ready");
    return len;
}

// Command to send data to the modem after the ">" symbol
// TODO: Think about custom handlers
int simcom_cmd_with_direct_payload(
    struct modem_data *mdata,
    const char *send_buf,
    const char *data,
    size_t data_len,
    const struct modem_cmd *one_handler_cmds,
    k_timeout_t timeout
)
{
    int ret;

    struct modem_cmd handler_cmds[] = {
        MODEM_CMD_DIRECT(">", on_cmd_tx_ready),
        MODEM_CMD_DIRECT(">", on_cmd_tx_ready)  // Fake record for one_handler_cmds
    };

    if(!simcom_STATUS_pin_is_present(mdata)) return -EIO;

    if(one_handler_cmds) {
        handler_cmds[1] = *one_handler_cmds;
    }

    /* Setup the locks correctly. */
    k_sem_take(&mdata->cmd_handler_data.sem_tx_lock, K_FOREVER);
    k_sem_reset(&mdata->sem_tx_ready);

    // /* set command handlers */
    // ret = modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
    //     handler_cmds, (one_handler_cmds) ? 2 : 1,
    //     true);
    // if(ret < 0) {
    //     LOG_ERR("simcom_cmd_with_direct_payload: Failed to set command handlers");
    //     goto exit;
    // }


    /* Send the Modem command. */
    ret = modem_cmd_send_ext(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
        handler_cmds, (one_handler_cmds) ? 2 : 1, send_buf, NULL, K_NO_WAIT,
        MODEM_NO_TX_LOCK | MODEM_NO_UNSET_CMDS
    );

    // ret = modem_cmd_send_nolock(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
    //     NULL, 0U, send_buf, NULL, K_NO_WAIT
    // );
    if(ret < 0) {
        LOG_ERR("simcom_cmd_with_direct_payload: Failed to send command");
        goto exit;
    }


    /* Wait for '>' */
    ret = k_sem_take(&mdata->sem_tx_ready, K_MSEC(5000));
    if (ret < 0) {
        /* Didn't get the data prompt - Exit. */
        LOG_ERR("simcom_cmd_with_direct_payload: Timeout waiting for tx [%s]", send_buf);
        goto exit;
    }

    /* Write all data on the console */
    mdata->mctx.iface.write(&mdata->mctx.iface, data, data_len);


    // Is it cover both scenarios?
    // OK, then Handler
    // Handler, then OK

    /* Wait for OK or ERROR */
    k_sem_reset(&mdata->sem_response);
    ret = k_sem_take(&mdata->sem_response, timeout);
    if (ret < 0) {
        LOG_ERR("simcom_cmd_with_direct_payload: No send response");
        goto exit;
    }
    ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
    if (ret < 0) {
        LOG_ERR("Got error: %d", ret);
        goto exit;
    }

    if(one_handler_cmds) {
        // Wait for second handler
        k_sem_reset(&mdata->sem_response);
        ret = k_sem_take(&mdata->sem_response, timeout);
        if (ret < 0) {
            LOG_ERR("simcom_cmd_with_direct_payload: No send response");
            goto exit;
        }
        ret = modem_cmd_handler_get_error(&mdata->cmd_handler_data);
        if (ret < 0) {
            LOG_ERR("Got error: %d", ret);
            goto exit;
        }
    }

    // TODO: Maybe need callback here?

exit:
    /* unset handler commands and ignore any errors */
    (void)modem_cmd_handler_update_cmds(&mdata->cmd_handler_data,
                        NULL, 0U, false);
    k_sem_give(&mdata->cmd_handler_data.sem_tx_lock);

    // if (ret < 0) {
    //     return ret;
    // }

    return ret;
}

// No operation command. Used for flushing putty buffer
int simcom_at(const struct device *dev)
{
    struct modem_data *mdata = dev->data;
    if(!simcom_STATUS_pin_is_present(mdata)) return -EIO;
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT", &mdata->sem_response, K_SECONDS(1));
}

// Free modem commad
int simcom_cmd(struct modem_data *mdata, const char *send_buf, k_timeout_t timeout)
{
    // char send_buf[sizeof("AT+CSSLCFG=\"cacert\",#,\"########################\"")] = {0};
    // snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"cacert\",%d,\"%s\"", ssl_ctx_index, ca_file);
    // struct modem_data *mdata = (struct modem_data *)dev->data;
    if(!simcom_STATUS_pin_is_present(mdata)) return -EIO;
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(10));
}
