
#ifdef WINNT
    #include "tinylib/net/udp_peer.h"
    #include <winsock2.h>
#elif defined(__linux__)
    #include "tinylib/linux/udp_peer.h"
#endif

#include "tinylib/util/log.h"

#include <stdlib.h>
#include <stdio.h>

static loop_t *g_loop;
static udp_peer_t *g_udp_peer;
static inetaddr_t g_remote_addr;

static 
void on_message(udp_peer_t *udp_peer, void *message, unsigned size, void* userdata, const inetaddr_t *peer_addr)
{
    return;
}

static char data[4096];
static 
void on_writable(udp_peer_t *udp_peer, void* userdata)
{
    udp_peer_send(g_udp_peer, data, sizeof(data), &g_remote_addr);
}

static 
void on_expire(void* userdata)
{
    loop_quit(g_loop);
    
    return;
}

int main(int argc, char *argv[])
{
    #ifdef WINNT
    WSADATA wsa_data;
    
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif
    
    if (argc < 2)
    {
        printf("usage: %s <remote ip>\n", argv[0]);
        return 0;
    }
    
    log_setlevel(LOG_LEVEL_DEBUG);

    inetaddr_initbyipport(&g_remote_addr, argv[1], 1994);

    g_loop = loop_new(1);
    g_udp_peer = udp_peer_new(g_loop, "0.0.0.0", 1994, on_message, NULL, NULL);
    udp_peer_onwrite(g_udp_peer, on_writable, NULL);
    (void)loop_runafter(g_loop, 360 * 1000, on_expire, NULL);
    udp_peer_expand_send_buffer(g_udp_peer, 2 * 1024 * 1024);

    loop_loop(g_loop);

    udp_peer_destroy(g_udp_peer);
    loop_destroy(g_loop);

    #ifdef WINNT
    WSACleanup();
    #endif
    
    return 0;
}
