#include "onoff.h"
#include "nvm/nvm.h"
#include "sms.h"
#include "gprs.h"
#include "tcp.h"
#include "http.h"
#include "id.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_sim7600g, CONFIG_MODEM_LOG_LEVEL);

// TODO: Refactor this!
extern volatile union _init_stages init_stages;

/* Func: pin_init
 * Desc: Boot up the Modem.
 */
static int pin_init(const struct modem_config *config)
{
    gpio_pin_configure_dt(&config->status, GPIO_INPUT);
	gpio_pin_configure_dt(&config->dtr, GPIO_OUTPUT);
	gpio_pin_set_dt(&config->dtr, 0);							// TODO: Set to 1 for entering sleep mode
	gpio_pin_configure_dt(&config->pwrkey, GPIO_OUTPUT);
	gpio_pin_set_dt(&config->pwrkey, 0);
	gpio_pin_configure_dt(&config->power, GPIO_OUTPUT);
	gpio_pin_set_dt(&config->power, 0);
    // TODO: Check is it work condition
    // #if DT_INST_NODE_HAS_PROP(0, discharge_gpios)
	gpio_pin_configure_dt(&config->discharge, GPIO_OUTPUT);
	gpio_pin_set_dt(&config->discharge, 0);
    // #endif

    // TODO: Use pins like this:
    /* MDM_POWER -> 1 for 500-1000 msec. */
	//modem_pin_write(&mctx, MDM_POWER, 1);
	//k_sleep(K_MSEC(750));

    k_msleep(100);
	LOG_DBG("gsm_start: Powering modem");
	gpio_pin_set_dt(&config->power, 1);
	k_msleep(1000);

	LOG_DBG("gsm_start: Pressing PWRKEY for 500ms");
	gpio_pin_set_dt(&config->pwrkey, 1);
	k_msleep(500);
	gpio_pin_set_dt(&config->pwrkey, 0);

	// uart_init(uart);

	LOG_INF("gsm_start: Waiting STATUS (typ 16s)");
	int timeout = 0;
	while(!gpio_pin_get_dt(&config->status) && timeout < 20) {
		timeout++;
		k_msleep(1000);
	}
	if(!gpio_pin_get_dt(&config->status)) {
		LOG_ERR("gsm_start: no STATUS signal");
		return -1;
	}
	LOG_DBG("gsm_start: STATUS goted for %d secs", timeout);
    return 0;
}

/* Commands sent to the modem to set it up at boot time. Before +CPIN: READY, SMS DONE and PB DONE */
static const struct setup_cmd setup1_cmds[] = {
	SETUP_CMD_NOHANDLE("AT&F"),
	SETUP_CMD_NOHANDLE("ATE0"),
	SETUP_CMD_NOHANDLE("AT+CMEE=0"),    // Disable result code,i.e. only “ERROR” will be displayed.
	// SETUP_CMD_NOHANDLE("AT+CFUN=1"),    Optional?
};

/* Commands sent to the modem to set it up after SIM card ready. */
static const struct setup_cmd setup2_cmds[] = {
    SETUP_CMD_NOHANDLE("AT+CMGF=1"),    // SMS format - Text
    SETUP_CMD_NOHANDLE("AT+CNMI=2,1,0,0"),    // New SMS message indicator
    SETUP_CMD_NOHANDLE("AT+CSDH=1"),
};

/* Func: modem_setup
 * Desc: This function is used to setup the modem from zero. The idea
 * is that this function will be called right after the modem is
 * powered on to do the stuff necessary to talk to the modem.
 */
int simcom_setup(struct modem_data *mdata, const struct modem_config *config)
{
    int ret;

    /* Setup the pins to ensure that Modem is enabled. */
    init_stages.byte = 0;
	pin_init(config);

    /* Let the modem respond. */
	LOG_INF("Waiting for modem to respond (RDY or any line?)");
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(60) /*MDM_MAX_BOOT_TIME*/);
	if (ret < 0) {
		LOG_ERR("Timeout waiting for RDY");
        return ret;
	}

    // Wait modem start response to simple "AT" command
    ret = -1;
    int counter = 0;
	while (counter++ < 5 && ret < 0) {
		k_sleep(K_SECONDS(1));
		ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
				     NULL, 0, "AT", &mdata->sem_response,
				     K_SECONDS(2));
		if (ret < 0 && ret != -ETIMEDOUT) {
			break;
		}
	}
    if(ret < 0) {
        LOG_ERR("Modem not responding to simple AT command");
        return ret;
    }

    ret = -1;
    counter = 0;
	while (counter++ < 5 && ret < 0) {
        /* Run setup commands on the modem. */
        ret = modem_cmd_handler_setup_cmds(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
                        setup1_cmds, ARRAY_SIZE(setup1_cmds),
                        &mdata->sem_response, MDM_REGISTRATION_TIMEOUT);
        if (ret >= 0) {
            break;
        }
    }

    // Wait +CPIN: READY or provocate it
    counter = 0;
	while (counter++ < 5) {
        if(mdata->cpin_ready) {
            break;
        }
        k_sleep(K_SECONDS(1));
        modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0, "AT+CPIN?", &mdata->sem_response, K_SECONDS(1));
    }

    // Wait PB DONE (longest event)
    counter = 0;
	while (counter++ < 10) {
        if(init_stages.bits.pb_done == 1) {
            break;
        }
        k_sleep(K_MSEC(250));
    }

    if(init_stages.bits.no_sim_card == 1) {
        LOG_ERR("No SIM card inserted.");
        return -1;
    }
    if(init_stages.bits.sms_done == 0) {
        LOG_ERR("SMS not ready. Its OK if you dont use SMS.");
        // return -1;
    }
    if(init_stages.bits.pb_done == 0) {
        LOG_ERR("Phonebook not ready. Its OK if you dont use Phonecalls.");
        // return -1;
    }

    ret = modem_cmd_handler_setup_cmds(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
                    setup2_cmds, ARRAY_SIZE(setup2_cmds),
                    &mdata->sem_response, MDM_REGISTRATION_TIMEOUT);
    if (ret < 0) {
        LOG_ERR("Failed to setup modem. Ignore for now.");
    }

    // if(!mdm_lte_cpin(mdata)) {
    //     LOG_ERR("Error LTE initialization. No SIM-card ready.");
    //     return -1;
    // }

    // Fix to T-Mobile
    // AT+COPS=1,2,"310260"
    #if defined(DEBUG_TMOBILE)
    modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+COPS=1,2,\"310260\"", &mdata->sem_response, K_SECONDS(5));
    k_sleep(K_SECONDS(20));
    #endif

    // #define APN "internet"
    // k_sleep(K_SECONDS(6));

    ret = query_ids(mdata);
    if (ret < 0) {
        k_sleep(K_SECONDS(1));
        ret = query_ids(mdata); // try again
        if (ret < 0) {  // REALY?
            LOG_ERR("Error quering LTE ID's (%d)", ret);
            k_sleep(K_SECONDS(5));
            query_ids(mdata); // try again
        }
		// goto error;
        return ret;
	}

    //k_sleep(K_SECONDS(60));

    modem_gprs_init(mdata);
    #if defined(CONFIG_MODEM_SMS)
        simcom_setup_sms_center(mdata);
    #endif
    #if defined(CONFIG_MODEM_TCP)
        simcom_tcp_context_enable(mdata);
    #endif
    #if defined(CONFIG_MODEM_HTTP)
        modem_http_init(mdata);
    #endif

    // LteCheckSMS(-1);  // Check all SMSs
    int i = -1;
    simcom_sms_read(&i, sizeof(i));

    if(mdata->event_cb) {
        mdata->event_cb(ATMODEM_EVENT_START, NULL, 0);
    }

    return 0;
}

int simcom_stop(struct modem_data *mdata, const struct modem_config *config)
{
    if(mdata->event_cb) {
        mdata->event_cb(ATMODEM_EVENT_STOP, NULL, 0);
    }

    #if defined(CONFIG_MODEM_HTTP)
        modem_http_done(mdata);
    #endif
    #if defined(CONFIG_MODEM_TCP)
        simcom_tcp_context_disable(mdata);
    #endif
    modem_gprs_done(mdata);

    #if 0
        LOG_ERR("LTE is temporrary disabled.");
        return 0;
    #endif
	if(!gpio_pin_get_dt(&config->status)) {
		LOG_WRN("No STATUS signal, modem is powered off?");
	} else {
		LOG_INF("gsm_stop: TODO: Execute AT+CPOF command");

		LOG_DBG("gsm_stop: Pressing PWRKEY for 2.5s");
		gpio_pin_set_dt(&config->pwrkey, 1);
		k_msleep(2500);
		gpio_pin_set_dt(&config->pwrkey, 0);

		LOG_DBG("gsm_stop: Waiting STATUS gone (typ 26s)");
		int timeout = 0;
		while(gpio_pin_get_dt(&config->status) && timeout < 40) {
			timeout++;
			k_msleep(1000);
		}
		if(gpio_pin_get_dt(&config->status)) {
			LOG_ERR("gsm_stop: STATUS is stil here");
		} else {
			LOG_DBG("gsm_stop: STATUS goted for %d sec", timeout);
		}
	}
	LOG_DBG("gsm_stop: Powering off");
	gpio_pin_set_dt(&config->power, 0);
	k_msleep(100);
	LOG_DBG("gsm_stop: Discharging");
	gpio_pin_set_dt(&config->discharge, 1);
	k_msleep(500);
	gpio_pin_set_dt(&config->discharge, 0);
	return 0;
}

/*
static void at_modem_on_work_handler(struct k_work *work)
{
    LOG_ERR("TODO: LTE MODEM ON HANDLER");
}

static K_WORK_DEFINE(at_modem_on_work, at_modem_on_work_handler);

int sim7600g_modem_on(const struct device *dev)
{
  	struct modem_data *data = (struct modem_data *)dev->data;

    return k_work_submit_to_queue(&data->workq, &at_modem_on_work);
}

static void at_modem_off_work_handler(struct k_work *work)
{
    LOG_ERR("TODO: LTE MODEM OFF HANDLER");
}

static K_WORK_DEFINE(at_modem_off_work, at_modem_off_work_handler);

int at_modem_off(const struct device *dev)
{
  	struct modem_data *data = (struct modem_data *)dev->data;

    return k_work_submit_to_queue(&data->workq, &at_modem_off_work);
}
*/


// Fast restart on 1st error
int modem_fast_reinit(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    LOG_ERR("TODO: LTE MODEM FAST REINIT (WIP)");

    if(mdata->event_cb) {
        mdata->event_cb(ATMODEM_EVENT_STOP, NULL, 0);
    }

    #if defined(CONFIG_MODEM_HTTP)
        modem_http_done(mdata);
    #endif
    #if defined(CONFIG_MODEM_TCP)
        simcom_tcp_context_disable(mdata);
    #endif
    modem_gprs_done(mdata);
    modem_gprs_init(mdata);
    #if defined(CONFIG_MODEM_SMS)
        simcom_setup_sms_center(mdata);
    #endif
    #if defined(CONFIG_MODEM_TCP)
        simcom_tcp_context_enable(mdata);
    #endif
    #if defined(CONFIG_MODEM_HTTP)
        modem_http_init(mdata);
    #endif

    if(mdata->event_cb) {
        mdata->event_cb(ATMODEM_EVENT_START, NULL, 0);
    }

    return 0;
}

// Full restart on 5 errors on a row
int modem_full_reinit(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    const struct modem_config *config = dev->config;

    LOG_ERR("LTE problems. Fast restart.");
    // modem_tcp_context_disable();        // No realy needed
    simcom_stop(mdata, config);
    k_sleep(K_SECONDS(5));
    int ret = simcom_setup(mdata, config);
    if(ret < 0) {
        LOG_ERR("Error LTE initialization. TODO: Off and ON?");
    }
    // naviccAddLogSimple(CONFIG_BOARD_PUPLIC_NAME " is on. Version: " APP_VERSION_STR);
    return 0;
}
