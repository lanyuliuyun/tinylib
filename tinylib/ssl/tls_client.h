
/** a basic  TLSv1.2 client implementation using openssl */

#ifndef TINYLIB_SSL_TLS_CLIENT_H
#define TINYLIB_SSL_TLS_CLIENT_H

typedef struct tls_client tls_client_t;

#ifdef WIN32
  #include "tinylib/windows/net/loop.h"
  #include "tinylib/windows/net/buffer.h"
#else
  #include "tinylib/linux/net/loop.h"
  #include "tinylib/linux/net/buffer.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*tls_client_on_connect_f)(tls_client_t* tls_client, int ok, void* userdata);
typedef void (*tls_client_on_data_f)(tls_client_t* tls_client, buffer_t* buffer, void* userdata);
typedef void (*tls_client_on_close_f)(tls_client_t* tls_client, void* userdata);

tls_client_t* tls_client_new(loop_t *loop, 
    const char *server_ip, unsigned short server_port, 
    tls_client_on_connect_f connectcb, tls_client_on_data_f datacb, 
    tls_client_on_close_f closecb, void *userdata,
    const char* ca_file, const char *private_key_file, const char *ca_pwd
);

void tls_client_destroy(tls_client_t* tls_client);

int tls_client_connect(tls_client_t* tls_client);

int tls_client_send(tls_client_t* tls_client, const void *data, unsigned size);

#ifdef __cplusplus
}
#endif

#endif // TINYLIB_SSL_TLS_CLIENT_H
