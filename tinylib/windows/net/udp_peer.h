
#ifndef UDP_PEER_H
#define UDP_PEER_H

struct udp_peer;
typedef struct udp_peer udp_peer_t;

#include "tinylib/windows/net/loop.h"
#include "tinylib/windows/net/inetaddr.h"

#include <winsock2.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*on_message_f)(udp_peer_t *peer, void *message, unsigned size, void* userdata, const inetaddr_t *peer_addr);

typedef void (*on_writable_f)(udp_peer_t *peer, void* userdata);

udp_peer_t* udp_peer_new(loop_t *loop, const char *ip, unsigned short port, on_message_f messagecb, on_writable_f writecb, void *userdata);

unsigned short udp_peer_getport(udp_peer_t* peer);

/* 挂接read事件，on_message_f为NULL时，表示清除read事件。返回原来的on_message_f */
on_message_f udp_peer_onmessage(udp_peer_t* peer, on_message_f messagecb, void *userdata);

/* 挂接write事件，writecb为NULL时，表示清除write事件。返回原来的wirtecb */
on_writable_f udp_peer_onwrite(udp_peer_t* peer, on_writable_f writecb, void *userdata);

void udp_peer_destroy(udp_peer_t* peer);

/* 由于udp的简单性，只做简单发送，请使用者自行完成报文分片，保证每次消息尺寸不超过65535 */
int udp_peer_send(udp_peer_t* peer, const void *message, unsigned len, const inetaddr_t *peer_addr);
int udp_peer_send2(udp_peer_t* peer, const void *message, unsigned len, const struct sockaddr_in *peer_addr);

/* 扩增发送buffer尺寸，每次以1K为单位向上圆整 */
void udp_peer_expand_send_buffer(udp_peer_t* peer, unsigned size);
void udp_peer_expand_recv_buffer(udp_peer_t* peer, unsigned size);

#ifdef __cplusplus
}
#endif

#endif /* !UDP_PEER_H */
