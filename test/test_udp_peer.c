
#if defined(WIN32)
  #include <winsock2.h>
#endif

#include "tinylib/net/udp_peer.h"
#include "tinylib/util/log.h"

#include <stdlib.h>
#include <stdio.h>

static loop_t *g_loop;
static udp_peer_t *g_udp_peer;

static 
void on_message(udp_peer_t *udp_peer, void *message, unsigned size, void* userdata, const inetaddr_t *peer_addr)
{
    fwrite(message, 1, size, stderr);
    fwrite("\n", 1, 1, stderr);

    return;
}

int main(int argc, char *argv[])
{
    #ifdef WIN32
    WSADATA wsa_data;
    
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif
    
    if (argc < 3)
    {
        printf("usage: %s <local ip> <local port>\n", argv[0]);
        return 0;
    }
    
    log_setlevel(LOG_LEVEL_DEBUG);

    g_loop = loop_new(1);
    g_udp_peer = udp_peer_new(g_loop, argv[1], (unsigned short)atoi(argv[2]), on_message, NULL, NULL);

    loop_loop(g_loop);

    udp_peer_destroy(g_udp_peer);
    loop_destroy(g_loop);

    #ifdef WIN32
    WSACleanup();
    #endif

    return 0;
}
