
/** 表示一个一个活动的tcp连接，
  * 不需要user进行connection的销毁操作，库本身会在合适的实际执行销毁
  */

#ifndef NET_TCP_CONNECTION_H
#define NET_TCP_CONNECTION_H

struct tcp_connection;
typedef struct tcp_connection tcp_connection_t;

#include "tinylib/windows/net/buffer.h"
#include "tinylib/windows/net/loop.h"
#include "tinylib/windows/net/inetaddr.h"

#include <winsock2.h>		/* fo SOCKET */

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*on_data_f)(tcp_connection_t* connection, buffer_t* buffer, void* userdata);
typedef void (*on_close_f)(tcp_connection_t* connection, void* userdata);

tcp_connection_t* tcp_connection_new(loop_t *loop, SOCKET fd, on_data_f datacb, on_close_f closecb, void* userdata, const inetaddr_t *peer_addr);

const inetaddr_t* tcp_connection_getpeeraddr(tcp_connection_t* connection);

const inetaddr_t* tcp_connection_getlocaladdr(tcp_connection_t* connection);

int tcp_connection_send(tcp_connection_t* connection, const void* data, unsigned size);

void tcp_connection_setcalback(tcp_connection_t* connection, on_data_f datacb, on_close_f closecb, void* userdata);

void tcp_connection_destroy(tcp_connection_t* connection);

/* 将所给的connection对象从其所属的loop中移出，从此该connection对象的IO事件将不再被监测 
 * 该方法不是线程安全的。
 */
void tcp_connection_detach(tcp_connection_t *connection);

/* 将所给的connection对象添加到指定的loop中进行IO事件监测，
 * 该方法是线程安全的 
 */
void tcp_connection_attach(tcp_connection_t *connection, loop_t *loop);

loop_t* tcp_connection_getloop(tcp_connection_t *connection);

int tcp_connection_connected(tcp_connection_t *connection);

void tcp_connection_expand_send_buffer(tcp_connection_t *connection, unsigned size);
void tcp_connection_expand_recv_buffer(tcp_connection_t *connection, unsigned size);

#ifdef __cplusplus
}
#endif

#endif /* !NET_TCP_CONNECTION_H */

