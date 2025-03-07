#include "mqtt.h"
#include "common.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_sim7600g, CONFIG_MODEM_LOG_LEVEL);

#include <zephyr/kernel.h>

// +CMQTTSTART: <err>
MODEM_CMD_DEFINE(on_cmd_mqtt_start)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, (argv[0][0] == '0') ? 0 : -EIO);
    k_sem_give(&mdata->sem_response);
    return 0;
}

static int mqtt_start(struct modem_data *mdata)
{
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTSTART: ", on_cmd_mqtt_start, 1U, "");
    return simcom_cmd_with_simple_wait_answer(mdata,
        "AT+CMQTTSTART", &handler_cmd, 1U, K_SECONDS(3)
    );
}

// Acquire a MQTT client
// AT+CMQTTACCQ=0, "client_id"
static int mqtt_acq(struct modem_data *mdata, const char* client_id)
{
    char send_buf[sizeof("AT+CMQTTACCQ=0, \"#######################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTACCQ=0, \"%s\"", client_id);
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
        NULL, 0U, send_buf, &mdata->sem_response, K_SECONDS(3)
    );
}

// AT+CMQTTCONNECT=0,"tcp://test.mosquitto. org:1883",60,1
// Wait +CMQTTCONNECT: 0,0
MODEM_CMD_DEFINE(on_cmd_mqtt_connect)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, (argv[1][0] == '0') ? 0 : -EIO);
    k_sem_give(&mdata->sem_response);
    return 0;
}

static int mqtt_connect(struct modem_data *mdata, const char* host, uint16_t port)
{
    char send_buf[sizeof("AT+CMQTTCONNECT=0,\"tcp://#######################:####\",60,1")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",30,1", host, port);
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTCONNECT: ", on_cmd_mqtt_connect, 2U, ",");
    return simcom_cmd_with_simple_wait_answer(mdata,
        send_buf, &handler_cmd, 1U, K_SECONDS(31)
    );
}

MODEM_CMD_DEFINE(on_cmd_mqtt_disconnect)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, 0);   //  Always OK
    k_sem_give(&mdata->sem_response);
    return 0;
}

// AT+CMQTTDISC=<client_index>,<timeout>
// Wait +CMQTTDISC: 0,0
static int mqtt_disconnect(struct modem_data *mdata)
{
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTDISC: ", on_cmd_mqtt_disconnect, 2U, ",");
    return simcom_cmd_with_simple_wait_answer(mdata,
        "AT+CMQTTDISC=0,30", &handler_cmd, 1U, K_SECONDS(31)
    );
}

// AT+CMQTTREL=0
static int mqtt_rel(struct modem_data *mdata)
{
    return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler,
        NULL, 0U, "AT+CMQTTREL=0", &mdata->sem_response, K_SECONDS(3));
}

// +CMQTTSTOP: 0
MODEM_CMD_DEFINE(on_cmd_mqtt_stop)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, 0);   //  Always OK
    k_sem_give(&mdata->sem_response);
    return 0;
}

static int mqtt_stop(struct modem_data *mdata)
{
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTSTOP: ", on_cmd_mqtt_stop, 1U, "");
    return simcom_cmd_with_simple_wait_answer(mdata,
        "AT+CMQTTSTOP", &handler_cmd, 1U, K_SECONDS(5)
    );
}

/* Handler: +CMQTTSUB: 0,0 */
MODEM_CMD_DEFINE(on_cmd_subed)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, (argv[1][0] == '0') ? 0 : -EIO);
    k_sem_give(&mdata->sem_response);
	return 0;
}

int simcom_mqtt_subscribe(struct modem_data *mdata, const char* topic, int qos)
{
    char send_buf[sizeof("AT+CMQTTSUB=0,###,1")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTSUB=0,%d,1", strlen(topic));
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTSUB: ", on_cmd_subed, 2U, ",");
    return simcom_cmd_with_direct_payload(mdata, send_buf, topic, strlen(topic), &handler_cmd, K_MSEC(5000));
}

// AT+CMQTTTOPIC=<client_index>,<req_length>
static int _mqtt_set_topic(struct modem_data *mdata, const char* topic)
{
    char send_buf[sizeof("AT+CMQTTTOPIC=0,###")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTTOPIC=0,%d", strlen(topic));
    return simcom_cmd_with_direct_payload(mdata, send_buf, topic, strlen(topic), NULL, K_MSEC(5000));
}

// AT+CMQTTPAYLOAD=<client_index>,<req_length>
static int _mqtt_set_payload(struct modem_data *mdata, const char* payload)
{
    char send_buf[sizeof("AT+CMQTTPAYLOAD=0,###")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTPAYLOAD=0,%d", strlen(payload));
    return simcom_cmd_with_direct_payload(mdata, send_buf, payload, strlen(payload), NULL, K_MSEC(5000));
}

// +CMQTTPUB: <client_index>,<err>
MODEM_CMD_DEFINE(on_cmd_mqtt_publish)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, 0);   //  Always OK? Or not?
    k_sem_give(&mdata->sem_response);
    return 0;
}

// AT+CMQTTPUB=<client_index>,<qos>,<pub_timeout>[,<retained>[,<dup>]]
static int _mqtt_publish(struct modem_data *mdata, uint8_t qos, uint16_t pub_timeout, bool retained)
{
    char send_buf[sizeof("AT+CMQTTPUB=0,#,######,0,0")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTPUB=0,%d,%d%s", qos, pub_timeout, retained ? ",1" : "");
    const struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTPUB: ", on_cmd_mqtt_publish, 2U, ",");
    return simcom_cmd_with_simple_wait_answer(mdata,
        send_buf,
        &handler_cmd, 1,
        K_SECONDS(10)
    );
}

int simcom_mqtt_publish(struct modem_data *mdata, const char* topic, const char* payload, uint8_t qos, uint16_t pub_timeout, bool retained)
{
    int ret;
    ret = _mqtt_set_topic(mdata, topic);
    if (ret < 0) return ret;
    ret = _mqtt_set_payload(mdata, payload);
    if (ret < 0) return ret;
    return _mqtt_publish(mdata, qos, pub_timeout, retained);
}

#define MQTT_RX_TOPIC_LEN 128
static char mqtt_rx_topic[MQTT_RX_TOPIC_LEN];
static size_t mqtt_rx_topic_len = 0;

#define MQTT_RX_BUF_LEN 2048
static uint8_t mqtt_rx_buf[MQTT_RX_BUF_LEN];
static size_t mqtt_rx_buf_len = 0;

int simcom_mqtt_connect(struct modem_data *mdata, const char* host, uint16_t port, const char* client_id)
{
    int ret;
    ret = mqtt_start(mdata);
    if (ret < 0) return ret;
    ret = mqtt_acq(mdata, client_id);
    if (ret < 0) return ret;
    ret = mqtt_connect(mdata, host, port);

    return ret;
}

int simcom_mqtt_disconnect(struct modem_data *mdata)
{
    int ret;
    ret = mqtt_disconnect(mdata);   // Maybe no need?
    ret = mqtt_rel(mdata);          // Release client
    ret = mqtt_stop(mdata);         // Stop MQTT service
    return 0;
}

void mqtt_on_rxstart(struct modem_data *mdata, int client_index, int topic_total_len, int payload_total_len)
{
    mqtt_rx_topic_len = 0;
    mqtt_rx_buf_len = 0;
}

// void mqtt_on_rxtopic(struct modem_data *mdata, int client_index, int sub_topic_len, char *sub_topic)
// {
//     LOG_ERR("on_cmd_mqtt_rxtopic: client_index=%d, sub_topic_len=%d, sub_topic=%s", client_index, sub_topic_len, sub_topic);
//     if (mqtt_rx_buf_len + sub_topic_len < MQTT_RX_BUF_LEN) {
//         memcpy(mqtt_rx_buf + mqtt_rx_buf_len, sub_topic, sub_topic_len);
//         mqtt_rx_buf_len += sub_topic_len;
//     }
// }

static int intlength(int v)
{
    if(v >= 1000) return 4;
    if(v >= 100) return 3;
    if(v >= 10) return 2;
    return 1;
}

// +CMQTTRXTOPIC: <client_index>,<sub_topic_len>\r\n<sub_topic>
int mqtt_on_rxtopic_handler(struct modem_cmd_handler_data *data, int client_index, int sub_topic_len, uint16_t len)
{
    int ret/*, packet_size*/;
    if (!len) {
		LOG_ERR("Invalid length, aborting");
		return -EAGAIN;
	}
    if (!data->rx_buf) {
		LOG_ERR("Incorrect format! Ignoring data!");
		return -EINVAL;
	}
    if (sub_topic_len <= 0) {
		LOG_ERR("Length error (%d)", sub_topic_len);
		return -EAGAIN;
	}
    int skips = intlength(sub_topic_len) + 2; // len(sub_topic_len) + (0x0A) + (0x0D)
    if (net_buf_frags_len(data->rx_buf) < (sub_topic_len + skips)) {
		LOG_DBG("Not enough data -- wait!");
		return -EAGAIN;
	}
    // Skip length value and 0x0A, 0x0D
    data->rx_buf = net_buf_skip(data->rx_buf, skips);
    //ret = net_buf_linearize(received_buffer, received_buffer_size, data->rx_buf, 0, (uint16_t)socket_data_length);
    ret = net_buf_linearize(
        mqtt_rx_topic + mqtt_rx_topic_len,
        MQTT_RX_TOPIC_LEN - mqtt_rx_topic_len,
        data->rx_buf, 0, (uint16_t)sub_topic_len
    );
    if (ret != sub_topic_len) {
		LOG_ERR("Total copied data is different then received data!"
			" copied:%d vs. received:%d",
			ret, sub_topic_len);
		ret = -EINVAL;
		goto exit;
	}
    data->rx_buf = net_buf_skip(data->rx_buf, ret/*+2*/);   // Skip last 0x0A, 0x0D ?
    mqtt_rx_topic_len += ret;
    ret = 0;
exit:
    return ret;
}

// +CMQTTRXPAYLOAD: <client_index>,<sub_payload_len>\r\n<sub_payload>
int mqtt_on_rx_handler(struct modem_cmd_handler_data *data, int client_index, int sub_payload_len, uint16_t len)
{
    int ret/*, packet_size*/;
    // uint8_t show_what_we_skip[64] = {0};
    if (!len) {
        LOG_ERR("Invalid length, aborting");
        return -EAGAIN;
    }
    if (!data->rx_buf) {
        LOG_ERR("Incorrect format! Ignoring data!");
        return -EINVAL;
    }
    if (sub_payload_len <= 0) {
        LOG_ERR("Length error (%d)", sub_payload_len);
        return -EAGAIN;
    }
    int skips = intlength(sub_payload_len) + 2; // len(sub_payload_len) + (0x0A) + (0x0D)
    if (net_buf_frags_len(data->rx_buf) < (sub_payload_len + skips)) {
        LOG_DBG("Not enough data -- wait!");
        return -EAGAIN;
    }
    // Skip length value and 0x0A, 0x0D
    // net_buf_linearize(show_what_we_skip, sizeof(show_what_we_skip), data->rx_buf, 0, (uint16_t)skips);
    // LOG_ERR("show_what_we_skip: [%s]", show_what_we_skip);
    data->rx_buf = net_buf_skip(data->rx_buf, skips);
    //ret = net_buf_linearize(received_buffer, received_buffer_size, data->rx_buf, 0, (uint16_t)socket_data_length);
    ret = net_buf_linearize(
        mqtt_rx_buf + mqtt_rx_buf_len,
        MQTT_RX_BUF_LEN - mqtt_rx_buf_len,
        data->rx_buf, 0, (uint16_t)sub_payload_len
    );
    if (ret != sub_payload_len) {
        LOG_ERR("Total copied data is different then received data!"
            " copied:%d vs. received:%d",
            ret, sub_payload_len);
        ret = -EINVAL;
        goto exit;
    }
    data->rx_buf = net_buf_skip(data->rx_buf, ret/*+2*/);   // Skip last 0x0A, 0x0D ?
    mqtt_rx_buf_len += ret;
    ret = 0;
exit:
    return ret;
}

// TODO: Dirty hack. Temporary solution
void charger_on_mqtt_message(char *topic, char *payload);
void mqtt_on_rxend(struct modem_data *mdata, int client_index)
{
    // LOG_ERR("on_cmd_mqtt_rxend: client_index=%d", client_index);
    mqtt_rx_topic[mqtt_rx_topic_len] = 0;
    mqtt_rx_buf[mqtt_rx_buf_len] = 0;
    charger_on_mqtt_message(mqtt_rx_topic, mqtt_rx_buf);
}

// TODO: Dirty hack. Temporary solution
void charger_lost_connection();
void mqtt_on_connlost(struct modem_data *mdata, int client_index, int cause)
{
    LOG_ERR("on_cmd_mqtt_connlost: client_index=%d, cause=%d", client_index, cause);
    // TODO: temporary solution. dirty hack
    charger_lost_connection();

}

// Example of incoming message
// Message: test message
// Topic: charger/id001/commands
//
// +CMQTTRXSTART: 0,22,12
// +CMQTTRXTOPIC: 0,22
// charger/id001/commands
// +CMQTTRXPAYLOAD: 0,12
// test message
// +CMQTTRXEND: 0
//
// Example of long incoming message
//
// +CMQTTRXSTART: 0,22,1520
// +CMQTTRXTOPIC: 0,22
// charger/id001/commands
// +CMQTTRXPAYLOAD: 0,1500
// .....
// +CMQTTRXPAYLOAD: 0,20
// .....
// +CMQTTRXEND: 0


// Unsolicited codes
//
// +CMQTTCONNLOST: <client_index>,<cause>
// When client disconnect passively, URC "+CMQTTCONNLOST"will be reported, then user need to connect MQTT server again.
//
// +CMQTTPING: <client_index>,<err>
// When send ping (which keep-alive to the server) to server failed,
// the module will report this URC.
// If received this message, you should disconnect the connectionand re-connect
//
// +CMQTTNONET
// When the network is become no network, the modulewill report this URC.
// If received this message, you should restart the MQTTservicebyAT+CMQTTSTART.
//
// +CMQTTRXSTART: <client_index>,<topic_total_len>,<payload_total_len>
// At the beginning of receiving published message, the modulewill
// report this to user, and indicate client index with <client_index>,
// the topic total length with <topic_total_len> and the payloadtotal
// length with <payload_total_len>.
//
// +CMQTTRXTOPIC: <client_index>,<sub_topic_len>\r\n<sub_topic>
// For long topic, it will be split to multiple packets to report
// andthecommand "+CMQTTRXTOPIC" will be send more thanoncewiththe
// rest of topic content. The sum of <sub_topic_len>is equal to
// <topic_total_len>.
//
// +CMQTTRXPAYLOAD: <client_index>,<sub_payload_len>\r\n<sub_payload>
// For long payload, the same as "+CMQTTRXTOPIC".
// +CMQTTRXEND: <client_index>
