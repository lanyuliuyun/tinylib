
#ifndef NET_TCP_CLIENT_H
#define NET_TCP_CLIENT_H

struct tcp_client;
typedef struct tcp_client tcp_client_t;

#include "net/tcp_connection.h"
#include "net/loop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*on_connected_f)(tcp_connection_t* connection, void *userdata);

tcp_client_t* tcp_client_new(loop_t *loop, const char* ip, unsigned short port, 
                    on_connected_f connectedcb, on_data_f datacb, on_close_f closecb, void* userdata);

int tcp_client_connect(tcp_client_t* client);

tcp_connection_t* tcp_client_getconnection(tcp_client_t* client);

void tcp_client_destroy(tcp_client_t* client);

#ifdef __cplusplus
}
#endif

#endif /* !NET_TCP_CLIENT_H */
