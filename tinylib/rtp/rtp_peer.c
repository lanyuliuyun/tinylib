
#include "rtp_peer.h"
#include "rtp_rtcp_packet.h"

#include "tinylib/util/util.h"
#include "tinylib/util/log.h"
#include "tinylib/util/atomic.h"

#include <stdlib.h>
#include <string.h>

struct rtp_peer
{
    loop_t* loop;
    unsigned short rtp_port;
    unsigned short rtcp_port;
    udp_peer_t* rtp_udppeer;
    udp_peer_t* rtcp_udppeer;
    unsigned index;
};

static struct rtp_peer_pool
{
    unsigned short start_port;
    unsigned peer_count;
    atomic_t *peer_bitmap;
}g_rtp_peer_pool;

void rtp_peer_pool_init(unsigned short start_port, unsigned peer_count)
{
    memset(&g_rtp_peer_pool, 0, sizeof(g_rtp_peer_pool));
    g_rtp_peer_pool.start_port = start_port;
    g_rtp_peer_pool.peer_count = peer_count;
    g_rtp_peer_pool.peer_bitmap = (atomic_t*)calloc(1, peer_count * sizeof(atomic_t));

    return;
}

void rtp_peer_pool_uninit(void)
{
    free(g_rtp_peer_pool.peer_bitmap);
    g_rtp_peer_pool.peer_bitmap = NULL;
    memset(&g_rtp_peer_pool, 0, sizeof(g_rtp_peer_pool));
    return;
}

/* 在给定的ip上分配一个rtp_peer */
rtp_peer_t* rtp_peer_alloc
(
    loop_t* loop, const char *ip, 
    on_message_f rtpcb, on_writable_f rtpwritecb, 
    on_message_f rtcpcb, on_writable_f rtcpwritecb, void* userdata
)
{
    rtp_peer_t* peer;
    udp_peer_t* rtp_udppeer;
    udp_peer_t* rtcp_udppeer;
    unsigned index;

    if (NULL == loop || NULL == ip || NULL == rtpcb || NULL == rtcpcb)
    {
        log_error("rtp_peer_alloc: bad loop(%p) or bad ip(%p) or bad rtpcb(%p) or bad rtcpcb(%p)", 
            loop, ip, rtpcb, rtcpcb);
        return NULL;
    }

    if (NULL == g_rtp_peer_pool.peer_bitmap)
    {
        return NULL;
    }

    peer = (rtp_peer_t*)calloc(1, sizeof(*peer));

    rtp_udppeer = NULL;
    rtcp_udppeer = NULL;
    for (index = 0; index < g_rtp_peer_pool.peer_count; ++index)
    {
        if (atomic_cas(g_rtp_peer_pool.peer_bitmap+index, 0, 1) != 0)
        {
            continue;
        }

        rtp_udppeer = udp_peer_new(loop, ip, g_rtp_peer_pool.start_port+(index<<1), rtpcb, rtpwritecb, userdata);
        if (NULL == rtp_udppeer)
        {
            (void)atomic_set(g_rtp_peer_pool.peer_bitmap+index, 0);
            continue;
        }

        rtcp_udppeer = udp_peer_new(loop, ip, g_rtp_peer_pool.start_port+(index<<1)+1, rtcpcb, rtcpwritecb, userdata);
        if (NULL == rtcp_udppeer)
        {
            udp_peer_destroy(rtp_udppeer);
            rtp_udppeer = NULL;
            (void)atomic_set(g_rtp_peer_pool.peer_bitmap+index, 0);
            continue;
        }

        break;
    }

    if (index == g_rtp_peer_pool.peer_count)
    {
        free(peer);
        return NULL;
    }

    peer->loop = loop;
    peer->rtp_port = g_rtp_peer_pool.start_port+(index<<1);
    peer->rtcp_port = g_rtp_peer_pool.start_port+(index<<1)+1;
    peer->rtp_udppeer = rtp_udppeer;
    peer->rtcp_udppeer = rtcp_udppeer;
    peer->index = index;

    return peer;
}

void rtp_peer_free(rtp_peer_t* peer)
{
    if (NULL == peer)
    {
        return;
    }

    udp_peer_destroy(peer->rtp_udppeer);
    udp_peer_destroy(peer->rtcp_udppeer);
    (void)atomic_set(g_rtp_peer_pool.peer_bitmap+peer->index, 0);
    free(peer);

    return;
}

unsigned short rtp_peer_rtpport(rtp_peer_t* peer)
{
    return NULL == peer ? 0 : peer->rtp_port;
}

unsigned short rtp_peer_rtcpport(rtp_peer_t* peer)
{
    return NULL == peer ? 0 : peer->rtcp_port;
}

udp_peer_t* rtp_peer_get_rtp_udppeer(rtp_peer_t* peer)
{
    return NULL == peer ? NULL : peer->rtp_udppeer;
}

udp_peer_t* rtp_peer_get_rtcp_udppeer(rtp_peer_t* peer)
{
    return NULL == peer ? NULL : peer->rtcp_udppeer;
}

static inline 
void build_default_bye_rtcp(rtcp_head_t *rtcp)
{
    memset(rtcp, 0, sizeof(*rtcp));
    rtcp->V = 2;
    rtcp->P = 0;
    rtcp->RC = 0;
    rtcp->PT = 203;
    rtcp->length = 0;

    return;
}

void rtp_peer_bye(rtp_peer_t* peer, const inetaddr_t *peer_addr)
{
    rtcp_head_t rtcp;

    if (NULL == peer || NULL == peer_addr)
    {
        return;
    }
    
    build_default_bye_rtcp(&rtcp);
    udp_peer_send(peer->rtcp_udppeer, &rtcp, sizeof(rtcp), peer_addr);

    return;
}
