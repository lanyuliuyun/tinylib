
/** a basic TLSv1.2 client implementation using openssl */

#ifndef TINYLIB_SSL_TLS_CLIENT_H
#define TINYLIB_SSL_TLS_CLIENT_H

typedef struct tls_client tls_client_t;

#include "tinylib/net/loop.h"
#include "tinylib/net/buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tls_client_on_connect_f)(tls_client_t* tls_client, int ok, void* userdata);
typedef void (*tls_client_on_data_f)(tls_client_t* tls_client, buffer_t* buffer, void* userdata);
typedef void (*tls_client_on_close_f)(tls_client_t* tls_client, void* userdata);

tls_client_t* tls_client_new
(
    loop_t *loop, const char *server_ip, unsigned short server_port, 
    tls_client_on_connect_f connectcb, tls_client_on_data_f datacb, tls_client_on_close_f closecb, void *userdata
);

/* 根据需要，指定client端使用的证书，仅支持PEM格式 
 * 如果key文件和cert文件是分离的，请通过private_key_file额外提供
 * 如果证书是加密过的，请通过ca_pwd提供密码
 */
int tls_client_use_ca(tls_client_t* tls_client, const char* ca_file, const char *private_key_file, const char *ca_pwd);

void tls_client_destroy(tls_client_t* tls_client);

int tls_client_connect(tls_client_t* tls_client);

int tls_client_send(tls_client_t* tls_client, const void *data, unsigned size);

#ifdef __cplusplus
}
#endif

#endif // TINYLIB_SSL_TLS_CLIENT_H
