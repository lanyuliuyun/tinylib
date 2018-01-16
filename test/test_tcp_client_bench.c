
#if defined(WIN32)
  #include <winsock2.h>
#endif

#include "tinylib/net/tcp_client.h"

#include <stdio.h>
#include <stdlib.h>

static loop_t *g_loop = NULL;
loop_timer_t *timer = NULL;

static
int random_packet[40960];
static
void on_interval(void *userdata)
{
    tcp_connection_t *connection = (tcp_connection_t*)userdata;
    tcp_connection_send(connection, random_packet, sizeof(random_packet));

    return;
}

static 
void on_data(tcp_connection_t* connection, buffer_t* buffer, void* userdata)
{
    buffer_retrieveall(buffer);
    return;
}

static 
void on_close(tcp_connection_t* connection, void* userdata)
{
    const inetaddr_t* addr = tcp_connection_getpeeraddr(connection);
    printf("connection to %s:%u will be closed\n", addr->ip, addr->port);
    loop_quit(g_loop);

    return;
}

static 
void on_connected(tcp_connection_t* connection, void *userdata)
{
    timer = loop_runevery(g_loop, 10, on_interval, connection);
    return;
}

int main(int argc, char *argv[])
{
    tcp_client_t* client1;

  #ifdef WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
  #endif

    g_loop = loop_new(1);

    client1 = tcp_client_new(g_loop, argv[1], (unsigned short)atoi(argv[2]), on_connected, on_data, on_close, NULL);
    tcp_client_connect(client1);

    loop_loop(g_loop);

    tcp_client_destroy(client1);
    loop_cancel(g_loop, timer);
    loop_destroy(g_loop);

  #ifdef WIN32
    WSACleanup();
  #endif

    return 0;
}
