#include "ble.h"
#include "ble/at.h"
#include "ble/id.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_esp_at, CONFIG_MODEM_LOG_LEVEL);


// GATTS Creates Services
int esp32_ble_gatts_create_services(struct modem_data *mdata)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+BLEGATTSSRVCRE", &mdata->sem_response, K_SECONDS(2));
}

// GATTS Starts Services
int esp32_ble_gatts_start_services(struct modem_data *mdata)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, "AT+BLEGATTSSRVSTART", &mdata->sem_response, K_SECONDS(2));
}

// Set Bluetooth LE Device Name
int esp32_ble_set_device_name(struct modem_data *mdata, const char* name)
{
    char send_buf[sizeof("AT+BLENAME=\"################################################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+BLENAME=\"%s\"", name);
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

int esp32_ble_start_server(struct modem_data *mdata, const char* name)
{
    int ret;

    ret = esp32_ble_init(mdata, BLE_ROLE_SERVER);
    if(ret < 0) {      // AT+BLEINIT=2
        LOG_ERR("Error init BLE");
        return ret;
    };

    ret = esp32_get_mac(mdata);
    if(ret < 0) {      // AT+BLEADDR?
        LOG_ERR("Error get MAC");
        return ret;
    };

    // LOG_ERR("BLE MAC %s", esp32_get_mac_str(mdata));

    // Alternative (no)
    // AT+BLEADVDATAEX="Bike","5AF51515799EF495E544047551FAE10D","020106050942696B6511065AF51515799EF495E544047551FAE10D",1

    ret = esp32_ble_gatts_create_services(mdata);        // AT+BLEGATTSSRVCRE
    if(ret < 0) {
        LOG_ERR("Error create services");
        return ret;
    };
    ret = esp32_ble_gatts_start_services(mdata);         // AT+BLEGATTSSRVSTART
    if(ret < 0) {
        LOG_ERR("Error start services");
        return ret;
    };
    // bt_direct_cmd_TEST("AT+BLEGATTSSRV?"); k_sleep(K_MSEC(200));
    // bt_direct_cmd_TEST("AT+BLEADDR?");  k_sleep(K_MSEC(200));
    // bt_direct_cmd_TEST("AT+BLEGATTSCHAR?"); k_sleep(K_MSEC(500));

    ret = esp32_ble_set_device_name(mdata, name);
    if(ret < 0) {
        LOG_ERR("Error set device name");
        return ret;
    };
    return ret;
}

int esp32_ble_start_adv(struct modem_data *mdata)
{
    // "AT+BLEADVSTART"
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0, "AT+BLEADVSTART", &mdata->sem_response, K_SECONDS(1));

}

// Automatically Set Bluetooth LE Advertising Data
int esp32_ble_set_adv_data(struct modem_data *mdata, const char* dev_name, const char* uuid, const char* manufacturer_data, unsigned include_power)
{
    char send_buf[sizeof("AT+BLEADVDATAEX=\"################################################\",\"################################################\",\"################################################\",1")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+BLEADVDATAEX=\"%s\",\"%s\",\"%s\",%u", dev_name, uuid, manufacturer_data, include_power);
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(2));
}

int atmodem_ble_set_adv_data(const struct device *dev, const char* dev_name, const char* uuid, const char* manufacturer_data, unsigned include_power)
{
    struct modem_data *mdata = dev->data;
    return esp32_ble_set_adv_data(mdata, dev_name, uuid, manufacturer_data, include_power);
}
