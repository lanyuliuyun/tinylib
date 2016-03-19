
#ifndef RTP_PEER_H
#define RTP_PEER_H

#ifdef WINNT
    #include "tinylib/windows/net/udp_peer.h"
#elif defined(__linux__)
    #include "tinylib/linux/net/udp_peer.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

struct rtp_peer;
typedef struct rtp_peer rtp_peer_t;

/* 初始化一个rtp_peer池
 * start_port 指定所使用的监听端口的起始值
 * peer_count 指定最多有多少个rtp_peer，一个rtp_peer同时包含rtp/rtcp端点
 * 故而实际最多将占用peer_count * 2个UDP端口
 */
void rtp_peer_pool_init(unsigned short start_port, unsigned peer_count);
void rtp_peer_pool_uninit(void);

/* 在给定的ip上分配一个rtp_peer，rtp/rtcp端口是偶奇相邻的
 * rtpwritecb/rtcpwritecb为NULL时表示不感知对应消息的write事件，需要时请利用udp_peer_onwrite()自行进行挂接
 */
rtp_peer_t* rtp_peer_alloc
(
    loop_t* loop, const char *ip, 
    on_message_f rtpcb, on_writable_f rtpwritecb, 
    on_message_f rtcpcb, on_writable_f rtcpwritecb, void* userdata
);

void rtp_peer_free(rtp_peer_t* peer);

unsigned short rtp_peer_rtpport(rtp_peer_t* peer);

unsigned short rtp_peer_rtcpport(rtp_peer_t* peer);

udp_peer_t* rtp_peer_get_rtp_udppeer(rtp_peer_t* peer);

udp_peer_t* rtp_peer_get_rtcp_udppeer(rtp_peer_t* peer);

/* 向指定的地址发送一个RTCP BYE消息 */
void rtp_peer_bye(rtp_peer_t* peer, const inetaddr_t *peer_addr);

#ifdef __cplusplus
}
#endif

#endif /* !RTP_PEER_H */
