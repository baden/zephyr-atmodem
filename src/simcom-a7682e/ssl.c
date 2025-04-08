#include "ssl.h"
#include <stdlib.h>

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(modem_a7682e, CONFIG_MMODEM_LOG_LEVEL);

#include <zephyr/kernel.h>

// Set the SSL version of the specified SSL context
// AT+CSSLCFG="sslversion",<ssl_ctx_index>,<sslversion>
int simcom_ssl_set_version(const struct device *dev, int ssl_ctx_index, int ssl_version)
{
    char send_buf[sizeof("AT+CSSLCFG=\"sslversion\",#,#####")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"sslversion\",%d,%d", ssl_ctx_index, ssl_version);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}

// Set the authentication mode(verify server and client) of the specified SSL context
// AT+CSSLCFG="authmode",<ssl_ctx_index>,<authmode>
int simcom_ssl_set_authmode(const struct device *dev, int ssl_ctx_index, int authmode)
{
    char send_buf[sizeof("AT+CSSLCFG=\"authmode\",#,#####")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"authmode\",%d,%d", ssl_ctx_index, authmode);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}

// Set the server root CA of the specified SSL context
// AT+CSSLCFG="cacert",<ssl_ctx_index>,"<ca_file>"
int simcom_ssl_set_cacert(const struct device *dev, int ssl_ctx_index, const char *ca_file)
{
    char send_buf[sizeof("AT+CSSLCFG=\"cacert\",#,\"##########################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"cacert\",%d,\"%s\"", ssl_ctx_index, ca_file);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}

// Set the client certificate of the specified SSL context
// AT+CSSLCFG="clientcert",<ssl_ctx_index>,"<clientcert_file>"
int simcom_ssl_set_clientcert(const struct device *dev, int ssl_ctx_index, const char *clientcert_file)
{
    char send_buf[sizeof("AT+CSSLCFG=\"clientcert\",#,\"#####################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"clientcert\",%d,\"%s\"", ssl_ctx_index, clientcert_file);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}

// Set the client key of the specified SSL context
// AT+CSSLCFG="clientkey",<ssl_ctx_index>,"<clientkey_file>"
int simcom_ssl_set_clientkey(const struct device *dev, int ssl_ctx_index, const char *clientkey_file)
{
    char send_buf[sizeof("AT+CSSLCFG=\"clientkey\",#,\"#####################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"clientkey\",%d,\"%s\"", ssl_ctx_index, clientkey_file);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}


// Download a certificate
// AT+CCERTDOWN=<filename>,<len>
int simcom_ssl_download_cert(const struct device *dev, const char *filename, const char *ssl_ca_cert, size_t ssl_ca_cert_len)
{
    char send_buf[sizeof("AT+CCERTDOWN=\"##############################\",#####")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CCERTDOWN=\"%s\",%d", filename, ssl_ca_cert_len);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    // return modem_cmd_send(&mdata->mctx.iface, &mdata->mctx.cmd_handler, ssl_ca_cert, ssl_ca_cert_len, send_buf, &mdata->sem_response, K_SECONDS(10));
    return simcom_cmd_with_direct_payload(mdata, send_buf, ssl_ca_cert, ssl_ca_cert_len, NULL, K_MSEC(5000));
}

// Move the cert from file system to cert content
// AT+CCERTMOVE=<filename>
int simcom_ssl_certmove(const struct device *dev, const char *filename)
{
    char send_buf[sizeof("AT+CCERTMOVE=\"##############################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CCERTMOVE=\"%s\"", filename);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}

// AT+CCERTDELE="<filename>"
int simcom_ssl_certremove(const struct device *dev, const char *filename)
{
    char send_buf[sizeof("AT+CCERTDELE=\"##############################\"")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CCERTDELE=\"%s\"", filename);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}

// List certificates
// AT+CCERTLIST
int simcom_ssl_certlist(const struct device *dev)
{
    char send_buf[sizeof("AT+CCERTLIST")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CCERTLIST");
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}

int simcom_ssl_set_ignorertctime(const struct device *dev, int ssl_ctx_index)
{
    char send_buf[sizeof("AT+CSSLCFG=\"ignorertctime\",################")] = {0};
    // snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"ignorertctime\",%d,1", ssl_ctx_index);
    snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"ignorelocaltime\",%d,1", ssl_ctx_index);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}


int simcom_ssl_set_var(const struct device *dev, int ssl_ctx_index, const char *param, int value)
{
    char send_buf[sizeof("AT+CSSLCFG=\"###################\",#,###########################")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CSSLCFG=\"%s\",%d,%d", param, ssl_ctx_index, value);
    struct modem_data *mdata = (struct modem_data *)dev->data;
    return simcom_cmd(mdata, send_buf, K_SECONDS(10));
}

// Configure the SSL context
int simcom_ssl_configure(const struct device *dev, int ssl_ctx_index, const struct ssl_config *config)
{
    simcom_ssl_set_version(dev, ssl_ctx_index, config->version);
    simcom_ssl_set_authmode(dev, ssl_ctx_index, config->authmode);
    if(config->ca_file != NULL) {
        int ret = simcom_ssl_set_cacert(dev, ssl_ctx_index, config->ca_file);
        if(ret < 0) return ret;
    }

    if(config->clientcert_file != NULL) {
        int ret = simcom_ssl_set_clientcert(dev, ssl_ctx_index, config->clientcert_file);
        if(ret < 0) return ret;
    }

    if(config->clientkey_file != NULL) {
        int ret = simcom_ssl_set_clientkey(dev, ssl_ctx_index, config->clientkey_file);
        if(ret < 0) return ret;
    }
    return 0;
}

// AT+CCHSTART

// Wait +CCHSTART: <n>
MODEM_CMD_DEFINE(on_cmd_ssl_start)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, (argv[1][0] == '0') ? 0 : -EIO);
    k_sem_give(&mdata->sem_response);
    return 0;
}

int simcom_ssl_start(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    // char send_buf[sizeof("AT+CMQTTCONNECT=0,\"tcp://#######################:####\",60,1")] = {0};
    // snprintk(send_buf, sizeof(send_buf), "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",30,1", host, port);
    struct modem_cmd handler_cmd = MODEM_CMD("+CCHSTART: ", on_cmd_ssl_start, 1U, "");
    return simcom_cmd_with_simple_wait_answer(mdata, "AT+CCHSTART", &handler_cmd, 1U, K_SECONDS(31));
}

// AT+CCHSTOP

// Wait +CCHSTOP: <n>
MODEM_CMD_DEFINE(on_cmd_ssl_stop)
{
	struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, (argv[1][0] == '0') ? 0 : -EIO);
    k_sem_give(&mdata->sem_response);
    return 0;
}

int simcom_ssl_stop(const struct device *dev)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    // char send_buf[sizeof("AT+CMQTTCONNECT=0,\"tcp://#######################:####\",60,1")] = {0};
    // snprintk(send_buf, sizeof(send_buf), "AT+CMQTTCONNECT=0,\"tcp://%s:%d\",30,1", host, port);
    struct modem_cmd handler_cmd = MODEM_CMD("+CCHSTOP: ", on_cmd_ssl_stop, 1U, "");
    return simcom_cmd_with_simple_wait_answer(mdata, "AT+CCHSTOP", &handler_cmd, 1U, K_SECONDS(31));
}


// AT+CCHSSLCFG Set the SSL context
// It must becalledbefore AT+CCHOPEN and after AT+CCHSTART. The setting will be cleared after AT+CCHOPEN failed or AT+CCHCLOSE.
// AT+CCHSSLCFG=<session_id>,<ssl_ctx_index>

int simcom_ssl_set_context(const struct device *dev, int session_id, int ssl_ctx_index)
{
    char send_buf[sizeof("AT+CCHSSLCFG=##,##")] = {0};
    struct modem_data *mdata = (struct modem_data *)dev->data;
    snprintk(send_buf, sizeof(send_buf), "AT+CCHSSLCFG=%d,%d", session_id, ssl_ctx_index);
    return simcom_cmd(mdata, send_buf, K_SECONDS(3));
}

// AT+CCHOPEN Connect to server
// AT+CCHOPEN=<session_id>,<host>,<port>[,<client_type>,[<bind_port>]]
// <session_id> = The session index to operate. It’s from 0 to 1.
// <host>
//      The server address, maximum length is 256 bytes.
// <port>
//      The server port which to be connected, the range is from 1 to 65535.
// <client_type>
//      The type of client, default value is 2:
//          1 TCP client.
//          2 SSL/TLS client.
// <bind_port>
//      The local port for channel, the range is from 1 to 65535.

// Wait +CCHOPEN: <session_id>,<err>
// <err>
//      The result code: 0 is success. Other values are failure. Pleaserefer to chapter 19.3

MODEM_CMD_DEFINE(on_cmd_ssl_open)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    // int session_id = atoi(argv[1]);
    int err = atoi(argv[2]);
    // modem_cmd_handler_set_error(data, (argv[1][0] == '0') ? 0 : -EIO);
    modem_cmd_handler_set_error(data, (err==0) ? 0 : -EIO);
    k_sem_give(&mdata->sem_response);
    return 0;
}

int simcom_ssl_connect(const struct device *dev, int session_id, const char* host, int port)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    char send_buf[sizeof("AT+CCHOPEN=#,\"##################################\",#####")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CCHOPEN=%d,\"%s\",%d,2", session_id, host, port);

    struct modem_cmd handler_cmd = MODEM_CMD("+CCHOPEN: ", on_cmd_ssl_open, 2U, ",");
    return simcom_cmd_with_simple_wait_answer(mdata, send_buf, &handler_cmd, 1U, K_SECONDS(31));

    // return simcom_cmd(dev, send_buf);
}

// AT+CCHCLOSE Disconnect from server
// AT+CCHCLOSE=<session_id>,0

MODEM_CMD_DEFINE(on_cmd_ssl_close)
{
    struct modem_data *mdata = CONTAINER_OF(data, struct modem_data, cmd_handler_data);
    modem_cmd_handler_set_error(data, 0);
    k_sem_give(&mdata->sem_response);
    return 0;
}

int simcom_ssl_disconnect(const struct device *dev, int session_id)
{
    struct modem_data *mdata = (struct modem_data *)dev->data;
    char send_buf[sizeof("AT+CCHCLOSE=##")] = {0};
    snprintk(send_buf, sizeof(send_buf), "AT+CCHCLOSE=%d", session_id);

    struct modem_cmd handler_cmd = MODEM_CMD("+CCHCLOSE: ", on_cmd_ssl_close, 2U, ",");
    return simcom_cmd_with_simple_wait_answer(mdata, send_buf, &handler_cmd, 1U, K_SECONDS(31));
}

// AT+CCHSEND=<session_id>,<len>
int simcom_ssl_send(const struct device *dev, int session_id, const char* payload)
{
    char send_buf[sizeof("AT+CCHSEND=####,########")] = {0};
    struct modem_data *mdata = (struct modem_data *)dev->data;
    snprintk(send_buf, sizeof(send_buf), "AT+CCHSEND=%d,%d", session_id, strlen(payload));
    return simcom_cmd_with_direct_payload(mdata, send_buf, payload, strlen(payload), NULL, K_MSEC(5000));
}
