#pragma once

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/net/http/parser.h>

enum atmodem_event {
    ATMODEM_EVENT_START,        // After modem full start
    ATMODEM_EVENT_STOP,         // Before modem stop
    ATMODEM_EVENT_POWERDOWN,    // After modem powerdown
    ATMODEM_EVENT_INCOMING_SMS, // On incoming SMS
};

#define SEND_SMS_MAX_MESSAGE_LENGTH 150
struct sms_payload {
    char phone[16];
    char message[SEND_SMS_MAX_MESSAGE_LENGTH];
};

/* Default lengths of certain things. */
#define MDM_MANUFACTURER_LENGTH		  32
#define MDM_MODEL_LENGTH		  24
#define MDM_REVISION_LENGTH		  64
#define MDM_IMEI_LENGTH			  16
#define MDM_IMSI_LENGTH			  16
//#define MDM_ICCID_LENGTH		  32

struct modem_id_data {
    char mdm_manufacturer[MDM_MANUFACTURER_LENGTH];
    char mdm_model[MDM_MODEL_LENGTH];
    char mdm_revision[MDM_REVISION_LENGTH];
	char mdm_imei[MDM_IMEI_LENGTH];
	char mdm_ICCID[32];
    uint8_t MAC[6];

    #if defined(CONFIG_MODEM_SIM_NUMBERS)
    	char mdm_imsi[MDM_IMSI_LENGTH];
    	//char mdm_iccid[MDM_ICCID_LENGTH];
    #endif /* #if defined(CONFIG_MODEM_SIM_NUMBERS) */
};


typedef int (*atmodem_work_handler_t)(void *payload, size_t payload_size);
typedef int (*atmodem_event_cb_t)(enum atmodem_event event, void *payload, size_t payload_size);

enum atmodem_state {
    ATMODEM_STATE_OFF = 0,
    ATMODEM_STATE_ON
};

#if defined(CONFIG_MODEM_HTTP)
struct http_config {
    enum http_method method;
    const char *content_type;
    const char *accept_type;
    const char *user_data;
    const char *body;
    size_t data_size;
    int cid;
};

struct http_last_action_result {
    bool success;
    int method;
    int status_code;
    size_t datalen;
};
#endif

__subsystem struct atmodem_driver_api {
    // int (*on)(const struct device *dev);
    // int (*off)(const struct device *dev);
    int (*setup)(const struct device *dev);
    int (*stop)(const struct device *dev);
    int (*run)(const struct device *dev, atmodem_work_handler_t handler, void *payload, size_t payload_size);
    int (*work)(const struct device *dev, struct k_work_delayable *worker);
    int (*set_event_cb)(const struct device *dev, atmodem_event_cb_t cb);
    enum atmodem_state (*get_state)(const struct device *dev);
    struct modem_id_data *(*get_id)(const struct device *dev);

    #if defined(CONFIG_MODEM_HTTP)
    // High level functions
    int (*http)(const struct device *dev,
        const struct http_config *config,
        struct http_last_action_result *result,
        const char *url
    );
    int (*http_read)(const struct device *dev,
        char *response, size_t response_size, size_t *response_data_size
    );
    #endif
    #if defined(CONFIG_MODEM_DNS)
    int (*dns)(const struct device *dev, const char* domain_name, char* resolved_name);
    #endif
    #if defined(CONFIG_MODEM_TCP)
    int (*tcp_connect)(const struct device *dev, int socket_index, const char* host, uint16_t port);
    int (*tcp_send)(const struct device *dev, int socket_index, const char* data, size_t data_size);
    int (*tcp_recv)(const struct device *dev, int socket_index, char* data, size_t data_size, size_t* data_read);
    int (*tcp_close)(const struct device *dev, int socket_index);
    #endif

    #if defined(CONFIG_MODEM_SMS)
    // SMS
    int (*send_sms)(const struct device *dev, const char* phone, const char* message);
    #endif

    // WIFI
    int (*wifi_setup)(const struct device *dev, const char* sid, const char* password); // TODO: Replace with add_wifi, remove_wifi, etc.
    int (*wifi_on)(const struct device *dev);
    int (*wifi_off)(const struct device *dev);

    // BLE
    int (*ble_start_server)(const struct device *dev, const char* name);
    int (*ble_start_adv)(const struct device *dev);
    int (*ble_notify)(const struct device *dev, unsigned conn_index, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer);
    int (*ble_notify_subscribers)(const struct device *dev, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer);
    int (*ble_set_attr)(const struct device *dev, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer);

    // OTA
    int (*ota_start)(const struct device *dev, const char* url);

    // Deprecated API
    int (*tcp_context_enable)(const struct device *dev);
    int (*tcp_context_disable)(const struct device *dev);

    // Internally used. No not call from outside.
    int (*__handler_result_parser)(const struct device *dev, int ret);
};

//__syscall int atmodem_setup(const struct device *dev);

static inline int /*z_impl_*/atmodem_setup(const struct device *dev)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->setup) return -ENOSYS;
    return api->setup(dev);
}

static inline int atmodem_stop(const struct device *dev)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->stop) return -ENOSYS;
    return api->stop(dev);
}

// FIFO queue
static inline int atmodem_run(const struct device *dev, atmodem_work_handler_t handler, void *payload, size_t payload_size)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->run) return -ENOSYS;
    return api->run(dev, handler, payload, payload_size);
}

static inline int atmodem_work(const struct device *dev, struct k_work_delayable *worker)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->work) return -ENOSYS;
    return api->work(dev, worker);
}

static inline int atmodem_set_event_cb(const struct device *dev, atmodem_event_cb_t cb)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->set_event_cb) return -ENOSYS;
    return api->set_event_cb(dev, cb);
}

static inline struct modem_id_data *atmodem_get_id(const struct device *dev)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->get_id) return NULL;
    return api->get_id(dev);
}

#if defined(CONFIG_MODEM_HTTP)
static inline int atmodem_http(const struct device *dev,
        const struct http_config *config,
        struct http_last_action_result *result,
        const char *url
    )
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->http) return -ENOSYS;
    return api->http(dev, config, result, url);
}

static inline int atmodem_http_read(const struct device *dev,
    char *response, size_t response_size, size_t *response_data_size)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->http_read) return -ENOSYS;
    return api->http_read(dev, response, response_size, response_data_size);
}
#endif

#if defined(CONFIG_MODEM_DNS)
static inline int atmodem_dns(const struct device *dev, const char* domain_name, char* resolved_name)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->dns) return -ENOSYS;
    return api->dns(dev, domain_name, resolved_name);
}
#endif

#if defined(CONFIG_MODEM_TCP)
static inline int atmodem_tcp_connect(const struct device *dev, int socket_index, const char* host, uint16_t port)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->tcp_connect) return -ENOSYS;
    return api->tcp_connect(dev, socket_index, host, port);
}

static inline int atmodem_tcp_send(const struct device *dev, int socket_index, const char* data, size_t data_size)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->tcp_send) return -ENOSYS;
    return api->tcp_send(dev, socket_index, data, data_size);
}

static inline int atmodem_tcp_recv(const struct device *dev, int socket_index, char* data, size_t data_size, size_t* data_read)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->tcp_recv) return -ENOSYS;
    return api->tcp_recv(dev, socket_index, data, data_size, data_read);
}

static inline int atmodem_tcp_close(const struct device *dev, int socket_index)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->tcp_close) return -ENOSYS;
    return api->tcp_close(dev, socket_index);
}
#endif

#if defined(CONFIG_MODEM_SMS)
static inline int atmodem_send_sms(const struct device *dev, const char* phone, const char* message)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->send_sms) return -ENOSYS;
    return api->send_sms(dev, phone, message);
}
#endif

static inline int atmodem_wifi_setup(const struct device *dev, const char* sid, const char* password)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->wifi_setup) return -ENOSYS;
    return api->wifi_setup(dev, sid, password);
}

static inline int atmodem_wifi_on(const struct device *dev)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->wifi_on) return -ENOSYS;
    return api->wifi_on(dev);
}

static inline int atmodem_wifi_off(const struct device *dev)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->wifi_off) return -ENOSYS;
    return api->wifi_off(dev);
}

static inline int atmodem_ble_start_server(const struct device *dev, const char* name)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->ble_start_server) return -ENOSYS;
    return api->ble_start_server(dev, name);
}

static inline int atmodem_ble_start_adv(const struct device *dev)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->ble_start_adv) return -ENOSYS;
    return api->ble_start_adv(dev);
}

static inline int atmodem_ble_notify(const struct device *dev, unsigned conn_index, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->ble_notify) return -ENOSYS;
    return api->ble_notify(dev, conn_index, srv_index, char_index, length, buffer);
}

static inline int atmodem_ble_notify_subscribers(const struct device *dev, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->ble_notify_subscribers) return -ENOSYS;
    return api->ble_notify_subscribers(dev, srv_index, char_index, length, buffer);
}

static inline int atmodem_ble_set_attr(const struct device *dev, unsigned srv_index, unsigned char_index, unsigned length, uint8_t *buffer)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->ble_set_attr) return -ENOSYS;
    return api->ble_set_attr(dev, srv_index, char_index, length, buffer);
}

// OTA
static inline int atmodem_ota_start(const struct device *dev, const char* url)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->ota_start) return -ENOSYS;
    return api->ota_start(dev, url);
}

// Deprecated API
static inline int atmodem_tcp_context_enable(const struct device *dev)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->tcp_context_enable) return -ENOSYS;
    return api->tcp_context_enable(dev);
}

static inline int atmodem_tcp_context_disable(const struct device *dev)
{
    const struct atmodem_driver_api *api = (const struct atmodem_driver_api *)dev->api;
    if(!api->tcp_context_disable) return -ENOSYS;
    return api->tcp_context_disable(dev);
}

// void test_http();

// Мені це здається не дуже зручним, все проводити через API
// Чому не зробити просто функції, які викликаються з драйвера?

int modem_simcom_http_fileread(const struct device *dev, const char* filename);
