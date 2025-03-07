#include "../simcom-a7682e.h"
#include <stdint.h>

int simcom_ssl_set_version(const struct device *dev, int ssl_ctx_index, int ssl_version);
int simcom_ssl_set_authmode(const struct device *dev, int ssl_ctx_index, int authmode);
int simcom_ssl_set_cacert(const struct device *dev, int ssl_ctx_index, const char *ca_file);
int simcom_ssl_set_clientcert(const struct device *dev, int ssl_ctx_index, const char *clientcert_file);
int simcom_ssl_set_clientkey(const struct device *dev, int ssl_ctx_index, const char *clientkey_file);

#define SIMCOM_SSL_VERSION_SSL3_0   0
#define SIMCOM_SSL_VERSION_TLS1_0   1
#define SIMCOM_SSL_VERSION_TLS1_1   2
#define SIMCOM_SSL_VERSION_TLS1_2   3
#define SIMCOM_SSL_VERSION_ALL      4

#define SIMCOM_SSL_AUTHMODE_NONE    0
#define SIMCOM_SSL_AUTHMODE_SERVER  1
#define SIMCOM_SSL_AUTHMODE_BOTH    2
#define SIMCOM_SSL_AUTHMODE_CLIENT  3

struct ssl_config {
    int version;
    int authmode;
    const char *ca_file;
    const char *clientcert_file;
    const char *clientkey_file;
};
int simcom_ssl_configure(const struct device *dev, int ssl_ctx_index, const struct ssl_config *config);

int simcom_ssl_download_cert(const struct device *dev, const char* filename, const char *ssl_ca_cert, size_t ssl_ca_cert_len);
int simcom_ssl_certmove(const struct device *dev, const char *filename);
int simcom_ssl_certlist(const struct device *dev);
int simcom_ssl_set_ignorertctime(const struct device *dev, int ssl_ctx_index);
int simcom_ssl_set_var(const struct device *dev, int ssl_ctx_index, const char *param, int value);

int simcom_ssl_start(const struct device *dev);
int simcom_ssl_stop(const struct device *dev);
int simcom_ssl_set_context(const struct device *dev, int session_id, int ssl_ctx_index);
int simcom_ssl_connect(const struct device *dev, int session_id, const char* host, int port);
int simcom_ssl_disconnect(const struct device *dev, int session_id);
int simcom_ssl_send(const struct device *dev, int ssl_ctx_index, const char* payload);

