
/** a basic DTLSv1.0 client implementation using openssl */

#ifndef TINYLIB_SSL_DTLS_ENDPOINT_H
#define TINYLIB_SSL_DTLS_ENDPOINT_H

struct dtls_endoint;
typedef struct dtls_endoint dtls_endoint_t;

enum dtls_endpoint_mode {
    DTLS_ENDPOINT_MODE_CLIENT,
    DTLS_ENDPOINT_MODE_SERVER,
};

#include "tinylib/linux/net/loop.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 握手完成就绪，可以发送应用层数据了 */
typedef void (*dtls_endoint_on_handshanke_f)(dtls_endoint_t* dtls_endoint, int ok, void* userdata);

typedef void (*dtls_endoint_on_shutdown_f)(dtls_endoint_t* dtls_endoint, int normal, void* userdata);

/* 接收解密后的数据 */
typedef void (*dtls_endoint_on_message_f)(dtls_endoint_t* dtls_endoint, void *message, unsigned size, void* userdata);

typedef void (*dtls_endoint_on_writable_f)(dtls_endoint_t* dtls_endoint, void* userdata);

dtls_endoint_t* dtls_endoint_new(loop_t *loop, 
    const char *server_ip, unsigned short server_port, 
    const char *local_ip, unsigned short local_port, 
    dtls_endoint_on_handshanke_f handshakecb, dtls_endoint_on_shutdown_f shutdowncb, 
    dtls_endoint_on_message_f messagecb, dtls_endoint_on_writable_f writecb, void *userdata,
    const char* ca_file, const char *private_key_file, const char *ca_pwd,
    enum dtls_endpoint_mode mode
);

int dtls_endoint_set_mode(dtls_endoint_t* dtls_endoint, enum dtls_endpoint_mode mode);

int dtls_endoint_enable_srtp(dtls_endoint_t* dtls_endoint);

void dtls_endoint_destroy(dtls_endoint_t* dtls_endoint);

/* 启动握手过程 */
int dtls_endoint_start(dtls_endoint_t* dtls_endoint);

int dtls_endoint_send(dtls_endoint_t* dtls_endoint, const void *packet, unsigned size);

#ifdef __cplusplus
}
#endif

#endif // TINYLIB_SSL_DTLS_ENDPOINT_H
