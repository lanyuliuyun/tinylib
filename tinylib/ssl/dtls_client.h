
#ifndef TINYLIB_SSL_DTLS_CLIENT_H
#define TINYLIB_SSL_DTLS_CLIENT_H

typedef struct dtls_client dtls_client_t;

#ifdef WIN32
  #include "tinylib/windows/net/loop.h"
#else
  #include "tinylib/linux/net/loop.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* 握手完成就绪，可以发送应用层数据了 */
typedef void (*dtls_client_on_handshanke_f)(dtls_client_t* dtls_client, int ok, void* userdata);

typedef void (*dtls_client_on_shutdown_f)(dtls_client_t* dtls_client, int normal, void* userdata);

/* 接收解密后的数据 */
typedef void (*dtls_client_on_message_f)(dtls_client_t* dtls_client, void *message, unsigned size, void* userdata);

typedef void (*dtls_client_on_writable_f)(dtls_client_t* dtls_client, void* userdata);

dtls_client_t* dtls_client_new(loop_t *loop, 
    const char *server_ip, unsigned short server_port, 
    const char *local_ip, unsigned short local_port, 
    dtls_client_on_handshanke_f handshakecb, dtls_client_on_shutdown_f shutdowncb, 
    dtls_client_on_message_f messagecb, dtls_client_on_writable_f writecb, void *userdata,
    const char* ca_file, const char *private_key_file, const char *ca_pwd
);

void dtls_client_destroy(dtls_client_t* dtls_client);

/* 启动握手过程 */
int dtls_client_start(dtls_client_t* dtls_client);

int dtls_client_send(dtls_client_t* dtls_client, void *packet, unsigned size);

#ifdef __cplusplus
}
#endif

#endif // TINYLIB_SSL_DTLS_CLIENT_H
