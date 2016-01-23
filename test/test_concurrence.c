
#ifdef WINNT
	#include "tinylib/net/udp_peer.h"
	#include <winsock2.h>
#elif defined(__linux__)
	#include "tinylib/linux/udp_peer.h"
#endif

#include "tinylib/util/log.h"

#include <stdlib.h>

static void on_message(udp_peer_t *udp_peer, void *message, unsigned size, void* userdata, const inetaddr_t *peer_addr)
{
	udp_peer_send(udp_peer, message, size, peer_addr);
	
	return;
}

static void on_writable(udp_peer_t *peer, void* userdata)
{
	return;
}

int main(int argc, char *argv[])
{
	loop_t *loop;
	udp_peer_t *udp_peers[1024];
	int i;
	
	#ifdef WINNT
	WSADATA wsa_data;
	
	WSAStartup(MAKEWORD(2, 2), &wsa_data);
	#endif
	
	log_setlevel(LOG_LEVEL_DEBUG);
	
	loop = loop_new(1024);
	
	for (i = 0; i < 1024; ++i)
	{
		udp_peers[i] = udp_peer_new(loop, "0.0.0.0", 12000 + i, on_message, NULL, &udp_peers[i]);
		udp_peer_onwrite(udp_peers[i], on_writable, &udp_peers[i]);
	}

	loop_loop(loop);

	for (i = 0; i < 1024; ++i);
	{
		udp_peer_destroy(udp_peers[i]);
	}
	
	loop_destroy(loop);
	
	#ifdef WINNT
	WSACleanup();
	#endif
	
	return 0;
}
