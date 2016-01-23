#ifndef NET_TCP_SERVER_H
#define NET_TCP_SERVER_H

struct tcp_server;
typedef struct tcp_server tcp_server_t;

#include "net/tcp_connection.h"
#include "net/loop.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*on_connection_f)(tcp_connection_t* connection, void* userdata, const inetaddr_t* peer_addr);

tcp_server_t* tcp_server_new
(
    loop_t *loop, on_connection_f onconn, void *userdata, 
    unsigned short port, const char* ip
);

void tcp_server_destroy(tcp_server_t *server);

int tcp_server_start(tcp_server_t *server);

void tcp_server_stop(tcp_server_t *server);

#ifdef __cplusplus
}
#endif

#endif /* !NET_TCP_SERVER_H */
