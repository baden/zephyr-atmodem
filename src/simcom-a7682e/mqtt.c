#include <stdlib.h> // strtol
#include "mqtt.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

#include <zephyr/kernel.h>

// +CMQTTSTART: <err>
MODEM_CMD_DEFINE(on_cmd_mqtt_start)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, (argv[0][0] == '0') ? 0 : -EIO);
    k_sem_give(&mdata->sem_response2);
    LOG_ERR("on +CMQTTSTART: [%s]", argv[0]);
    return 0;
}

static int mqtt_start(struct modem_data *mdata)
{
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTSTART: ", on_cmd_mqtt_start, 1U, "");
    return simcom_cmd_with_simple_wait_answer(mdata,
        "AT+CMQTTSTART", &handler_cmd, 1U, K_SECONDS(3)
    );
}

// int simcom_mqtt_start(struct device *dev)
// {
//     struct modem_data *mdata = dev->data;
//     return mqtt_start(mdata);
// }

// Acquire a MQTT client
// AT+CMQTTACCQ=<client_index>,<clientID>[<server_type>]
static int mqtt_acq(struct modem_data *mdata, int client_index, const char* client_id, int server_type)
{
    char send_buf[sizeof("AT+CMQTTACCQ=#,\"##############################################\",#")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTACCQ=%d,\"%s\",%d", client_index, client_id, server_type);
    return simcom_cmd(mdata, send_buf, K_SECONDS(3));
}

// AT+CMQTTREL Release a client
static int mqtt_rel(struct modem_data *mdata)
{
    return simcom_cmd(mdata, "AT+CMQTTREL=0", K_SECONDS(3));
}

// AT+CMQTTCONNECT=0,"tcp://test.mosquitto. org:1883",60,1
// Wait +CMQTTCONNECT: 0,0
MODEM_CMD_DEFINE(on_cmd_mqtt_connect)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    int client_index = atoi(argv[0]);
    bool connected = (argv[1][0] == '0') ? true : false;
    LOG_ERR("DEBUG on +CMQTTCONNECT: client_index=%d, connected=%d", client_index, connected);
    modem_cmd_handler_set_error(data, connected ? 0 : -EIO);
    mdata->mqtt_states[client_index] = connected ? MQTT_STATE_CONNECTED : MQTT_STATE_DISCONNECTED;
    k_sem_give(&mdata->sem_response2);
    return 0;
}

static int mqtt_connect(struct modem_data *mdata, const char* host, uint16_t port)
{
    char send_buf[sizeof("AT+CMQTTCONNECT=0,\"tcp://#######################:####\",600,1")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",60,0", host, port);
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTCONNECT: ", on_cmd_mqtt_connect, 2U, ",");
    return simcom_cmd_with_simple_wait_answer(mdata,
        send_buf, &handler_cmd, 1U, K_SECONDS(31)
    );
}

// +CMQTTDISC: <client_index>,<err>
MODEM_CMD_DEFINE(on_cmd_mqtt_disconnect)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    int client_index = atoi(argv[0]);
    // int err = atoi(argv[1]);
    mdata->mqtt_states[client_index] = MQTT_STATE_DISCONNECTED;
    modem_cmd_handler_set_error(data, 0);   //  Always OK
    k_sem_give(&mdata->sem_response2);
    return 0;
}

// AT+CMQTTDISC=<client_index>,<timeout>
// Wait +CMQTTDISC: 0,0
static int mqtt_disconnect(struct modem_data *mdata)
{
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTDISC: ", on_cmd_mqtt_disconnect, 2U, ",");
    return simcom_cmd_with_simple_wait_answer(mdata,
        "AT+CMQTTDISC=0,60", &handler_cmd, 1U, K_SECONDS(31)
    );
}

// +CMQTTSTOP: 0
MODEM_CMD_DEFINE(on_cmd_mqtt_stop)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, 0);   //  Always OK
    k_sem_give(&mdata->sem_response2);
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

int simcom_mqtt_subscribe(const struct device *dev /*struct modem_data *mdata*/, const char* topic, int qos)
{
    char send_buf[sizeof("AT+CMQTTSUB=0,##########,1")] = {0};
    LOG_ERR("simcom_mqtt_subscribe: [%s]", topic);
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTSUB=0,%d,%d", strlen(topic), qos);
    struct modem_data *mdata = dev->data;
    struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTSUB: ", on_cmd_subed, 2U, ",");
    return simcom_cmd_with_direct_payload(mdata, send_buf, topic, strlen(topic), &handler_cmd, K_MSEC(5000));
}

// AT+CMQTTTOPIC=<client_index>,<req_length>
static int _mqtt_set_topic(struct modem_data *mdata, const char* topic)
{
    char send_buf[sizeof("AT+CMQTTTOPIC=0,##########")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTTOPIC=0,%d", strlen(topic));
    return simcom_cmd_with_direct_payload(mdata, send_buf, topic, strlen(topic), NULL, K_MSEC(5000));
}

// AT+CMQTTPAYLOAD=<client_index>,<req_length>
static int _mqtt_set_payload(struct modem_data *mdata, const char* payload)
{
    char send_buf[sizeof("AT+CMQTTPAYLOAD=0,##########")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTPAYLOAD=0,%d", strlen(payload));
    return simcom_cmd_with_direct_payload(mdata, send_buf, payload, strlen(payload), NULL, K_MSEC(5000));
}

// +CMQTTPUB: <client_index>,<err>
MODEM_CMD_DEFINE(on_cmd_mqtt_publish)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, 0);   //  Always OK? Or not?
    k_sem_give(&mdata->sem_response2);
    return 0;
}

// AT+CMQTTPUB=<client_index>,<qos>,<pub_timeout>[,<retained>[,<dup>]]
static int _mqtt_publish(struct modem_data *mdata, uint8_t qos, uint16_t pub_timeout, bool retained)
{
    char send_buf[sizeof("AT+CMQTTPUB=0,#,######,0,0")] = {0};
    if(pub_timeout < 60) pub_timeout = 60;
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTPUB=0,%d,%d%s", qos, pub_timeout, retained ? ",1" : "");
    // snprintk(send_buf, sizeof(send_buf), "AT+CMQTTPUB=0,%d,%d,%d", qos, pub_timeout, retained ? 1 : 0);
    // snprintk(send_buf, sizeof(send_buf), "AT+CMQTTPUB=?", qos, pub_timeout, retained ? ",1" : "");
    const struct modem_cmd handler_cmd = MODEM_CMD("+CMQTTPUB: ", on_cmd_mqtt_publish, 2U, ",");
    return simcom_cmd_with_simple_wait_answer(mdata,
        send_buf,
        &handler_cmd, 1,
        K_SECONDS(10)
    );
}

// Документация здається неправильно описана
// Судячи по прикладу, формат наступний:
// AT+CMQTTPUB Publish a message to server
// AT+CMQTTPUB=<client_index>,"<topic>",<qos>[,<req_length>[,<ratained>]]

int simcom_mqtt_publish(const struct device *dev/*struct modem_data *mdata*/, const char* topic, const char* payload, uint8_t qos, uint16_t pub_timeout, bool retained)
{
    int ret;
    struct modem_data *mdata = dev->data;
    // simcom_at(dev);
    ret = _mqtt_set_topic(mdata, topic);
    if (ret < 0) return ret;
    ret = _mqtt_set_payload(mdata, payload);
    simcom_at(dev);
    if (ret < 0) return ret;
    return _mqtt_publish(mdata, qos, pub_timeout, retained);
}
#if 0
int simcom_mqtt_set_topic(const struct device *dev, const char* topic)
{
    // int ret;
    struct modem_data *mdata = dev->data;
    return _mqtt_set_topic(mdata, topic);
}

int simcom_mqtt_payloay(const struct device *dev, const char* payload)
{
    int ret;
    struct modem_data *mdata = dev->data;
    ret = _mqtt_set_payload(mdata, payload);
    simcom_at(dev);
    return ret;
}

int simcom_mqtt_do_publish(const struct device *dev, uint8_t qos, uint16_t pub_timeout, bool retained)
{
    // int ret;
    struct modem_data *mdata = dev->data;
    return _mqtt_publish(mdata, qos, pub_timeout, retained);
}
#endif
#define MQTT_RX_TOPIC_LEN 128
static char mqtt_rx_topic[MQTT_RX_TOPIC_LEN];
static size_t mqtt_rx_topic_len = 0;

#define MQTT_RX_BUF_LEN 2048
static uint8_t mqtt_rx_buf[MQTT_RX_BUF_LEN];
static size_t mqtt_rx_buf_len = 0;

#define CONFIG_MQTT_USESSL 1

// Set the SSL context (only for SSL/TLS MQTT)
// AT+CMQTTSSLCFG=<session_id>,<ssl_ctx_index>
static int _mqtt_set_ssl_ctx(struct modem_data *mdata, int session_id, int ssl_ctx_index)
{
    char send_buf[sizeof("AT+CMQTTSSLCFG=#,#")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CMQTTSSLCFG=%d,%d", session_id, ssl_ctx_index);
    return simcom_cmd(mdata, send_buf, K_SECONDS(3));
}

int simcom_mqtt_start(const struct device *dev, const char* client_id)
{
    int ret;
    struct modem_data *mdata = dev->data;
    ret = mqtt_start(mdata);
    if (ret < 0) {
        ret = mqtt_stop(mdata);
        ret = mqtt_start(mdata);
        if (ret < 0) {
            // return ret;
        }
    }

    #if defined(CONFIG_MQTT_USESSL) && (CONFIG_MQTT_USESSL == 1)
        // Acquire one client which will connect to a SSL/TLS MQTT server
        ret = mqtt_acq(mdata, 0, client_id, 1); // SSL
        if (ret < 0) return ret;
        // Set the first SSL context to be used in the SSL connection
        // _mqtt_set_ssl_ctx(mdata, 0, CONFIG_MQTT_SSL_CTX_INDEX);
    #else
        // Acquire one client which will connect to a non-SSL/TLS MQTT server
        int ret = mqtt_acq(mdata, 0, client_id, 0); // Non-SSL
        if (ret < 0) return ret;
    #endif

    return 0;
}

int simcom_mqtt_stop(const struct device *dev)
{
    struct modem_data *mdata = dev->data;
    mqtt_rel(mdata);
    return mqtt_stop(mdata);
}

int simcom_mqtt_connect(const struct device *dev, const char* host, uint16_t port)
{
    struct modem_data *mdata = dev->data;

    #if defined(CONFIG_MQTT_USESSL) && (CONFIG_MQTT_USESSL == 1)
        // // Acquire one client which will connect to a SSL/TLS MQTT server
        // int ret = mqtt_acq(mdata, 0, client_id, 1); // SSL
        // if (ret < 0) return ret;
        // Set the first SSL context to be used in the SSL connection
        _mqtt_set_ssl_ctx(mdata, 0, CONFIG_MQTT_SSL_CTX_INDEX);
    #else
        // // Acquire one client which will connect to a non-SSL/TLS MQTT server
        // int ret = mqtt_acq(mdata, 0, client_id, 0); // Non-SSL
        // if (ret < 0) return ret;
    #endif

    // Set the will topic for the CONNECT message (TBD)
    // AT+CMQTTWILLTOPIC=0,10
    // Set the will message for the CONNECT message (TBD)
    // AT+CMQTTWILLMSG=0,6,1

    // Set the first SSL context to be used in the SSL connection
    return mqtt_connect(mdata, host, port);
}

// AT+CMQTTCONNECT?
// Очікуємо відповідь якшо немає з'єднання
// +CMQTTCONNECT: 0
// +CMQTTCONNECT: 1
//
// Очікуємо відповідь якшо є з'єднання
// +CMQTTCONNECT: 0,"tcp://5.187.3.28:8883",600,1
// +CMQTTCONNECT: 1
/* Handler: +CMQTTCONNECT: 0[,"tcp://5.187.3.28:8883",600,1] */
MODEM_CMD_DEFINE(on_cmd_cmqttconnect)
{
   	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);

    // LOG_ERR("(c:%d) 0:[%s] 1:[%s]", argc, argv[0], argv[1]);
    int client_index = atoi(argv[0]);

    if(argc == 1) {
        // No connection
        // modem_cmd_handler_set_error(data, 0);
        // k_sem_give(&mdata->sem_response);
        mdata->mqtt_states[client_index] = MQTT_STATE_DISCONNECTED;
        return 0;
    }

	/* Log the received information. */
	// LOG_INF("Model: %s", /*log_strdup(*/mdata.modem_id.mdm_model/*)*/);
	return 0;
}

int simcom_mqtt_connected(const struct device *dev, bool *connected)
{
    int ret = 0;
    struct modem_data *mdata = dev->data;
    // Сподіваюсь шо локальне перехоплення +CMQTTCONNECT: перекриє глобальне
    const struct modem_cmd cmd = MODEM_CMD_ARGS_MAX("+CMQTTCONNECT: ", on_cmd_cmqttconnect, 1U, 2U, ",");
    ret = modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, &cmd, 1U, "AT+CMQTTCONNECT?", &mdata->sem_response, K_SECONDS(3));
    if (connected) {
        *connected = (mdata->mqtt_states[0] == MQTT_STATE_CONNECTED);
    }
    return ret;
}

int simcom_mqtt_disconnect(const struct device *dev)
{
    int ret;
    struct modem_data *mdata = dev->data;
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
void lte_on_mqtt_message(char *topic, char *payload);
void mqtt_on_rxend(struct modem_data *mdata, int client_index)
{
    // LOG_ERR("on_cmd_mqtt_rxend: client_index=%d", client_index);
    mqtt_rx_topic[mqtt_rx_topic_len] = 0;
    mqtt_rx_buf[mqtt_rx_buf_len] = 0;

    // Повна хуйня. Мені це взагалі не подобається.
    // Треба якось це зробити по нормальному
    lte_on_mqtt_message(mqtt_rx_topic, mqtt_rx_buf);
}

// TODO: Dirty hack. Temporary solution!!!
void lte_mqtt_lost_connection();
void mqtt_on_connlost(struct modem_data *mdata, int client_index, int cause)
{
    mdata->mqtt_states[client_index] = MQTT_STATE_DISCONNECTED;
    LOG_ERR("on_cmd_mqtt_connlost: client_index=%d, cause=%d", client_index, cause);
    // TODO: temporary solution. dirty hack
    lte_mqtt_lost_connection();
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
