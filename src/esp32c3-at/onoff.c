#include "onoff.h"

#include <zephyr/device.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_esp_at, CONFIG_MODEM_LOG_LEVEL);

static void pin_init(const struct modem_config *config)
{
    gpio_pin_configure_dt(&config->bt_on, GPIO_OUTPUT);
	gpio_pin_set_dt(&config->bt_on, 1);    // Powering ON module
}

/* Commands sent to the modem to set it up at boot time. */
static const struct setup_cmd setup_cmds[] = {
	SETUP_CMD_NOHANDLE("ATE0"),
    SETUP_CMD_NOHANDLE("AT+CWMODE=0"),  // Disable WiFi by default
    SETUP_CMD_NOHANDLE("AT+DRVPWMINIT=5000,13,2"),
    SETUP_CMD_NOHANDLE("AT+DRVPWMDUTY=0"),
    SETUP_CMD_NOHANDLE("AT+DRVPWMFADE=512,4000"),
};

/* Func: modem_setup
 * Desc: This function is used to setup the modem from zero. The idea
 * is that this function will be called right after the modem is
 * powered on to do the stuff necessary to talk to the modem.
 */
int esp32_setup(struct modem_data *mdata, const struct modem_config *config)
{
    // int ret;
    pin_init(config);

    // All rest tasks are done after receiving 'ready' from the modem
#if 0
again:
    _wait_ready = true;
    LOG_INF("Waiting for modem to respond ('ready' or any line?)");
    // k_sleep(K_SECONDS(5));
	ret = k_sem_take(&mdata->sem_response, K_SECONDS(10) /*MDM_MAX_BOOT_TIME*/);
    _wait_ready = false;
	if (ret < 0) {
		LOG_ERR("Timeout waiting for ready. Restarting BT module.");
        // Power off & power on & repeat
        gpio_pin_set_dt(&config->bt_on, 0);
        k_sleep(K_SECONDS(5));
        gpio_pin_set_dt(&config->bt_on, 1);
        goto again;
        // return ret;
	}
#endif
    return 0;
}

int esp32_stop(struct modem_data *mdata, const struct modem_config *config)
{
    if(mdata->event_cb) {
        mdata->event_cb(ATMODEM_EVENT_STOP, NULL, 0);
    }
    // TODO: Stop all NET stacks
    gpio_pin_configure_dt(&config->bt_on, GPIO_OUTPUT);
	gpio_pin_set_dt(&config->bt_on, 0);
    return 0;
}

// Call after got "ready" from modem
int esp32_init(struct modem_data *mdata)
{
    k_sleep(K_SECONDS(1));
    // Wait modem start response to simple "AT" command
    int ret = -1;
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

    /* Run setup commands on the modem. */
	ret = modem_cmd_handler_setup_cmds(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
					   setup_cmds, ARRAY_SIZE(setup_cmds),
					   &mdata->sem_response, MDM_REGISTRATION_TIMEOUT);
	if (ret < 0) {
        LOG_ERR("Error configuring BT (%d)", ret);
		// goto error;
        return ret;
	}

    // TODO: Start all NET stacks

    if(mdata->event_cb) {
        mdata->event_cb(ATMODEM_EVENT_START, NULL, 0);
    }

    return 0;
}

// Dirty restart on failure
int esp32_restart(struct modem_data *mdata)
{
    // struct modem_data *mdata = (struct modem_data *)dev->data;
    // const struct device *dev1 = CONTAINER_OF(mdata, struct device, dev);
    // // struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // const struct modem_config *config = dev1->config;

    // LOG_ERR("esp32_restart caller: dev1=%p, mdata=%p, config=%p", dev1, mdata, config);

    // espressif_esp32c3at
    const struct device *dev2 = DEVICE_DT_GET(DT_ALIAS(ble));
    struct modem_data *mdata1 = dev2->data;
    const struct modem_config *config1 = dev2->config;

    LOG_ERR("esp32_restart driver: dev2=%p, mdata1=%p, config1=%p", dev2, mdata1, config1);

    esp32_stop(mdata, config1);
    k_sleep(K_SECONDS(1));
    esp32_setup(mdata, config1);
    // gpio_pin_set_dt(&config->bt_on, 1);
    return 0;
}

int esp32_restart_dev(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return esp32_restart(mdata);
}
