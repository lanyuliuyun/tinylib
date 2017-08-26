
#ifdef WIN32
    #include "tinylib/windows/net/tcp_client.h"
    #include <winsock2.h>
#elif defined(__linux__)
    #include "tinylib/linux/net/tcp_client.h"
#endif

#include <stdio.h>
#include <string.h>
#include <assert.h>

static loop_t *g_loop = NULL;

static 
void on_data(tcp_connection_t* connection, buffer_t* buffer, void* userdata)
{
    const inetaddr_t* addr = tcp_connection_getpeeraddr(connection);
    printf("%u bytes recevied from %s:%u\n", buffer_readablebytes(buffer), addr->ip, addr->port);
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
    char msg[5000] = "1234567890POIUYTREWQASDFGHJKLMNBVCXZ\n";

    tcp_connection_send(connection, msg, sizeof(msg));

    return;
}

int main(int argc, char *argv[])
{
    tcp_client_t* client1;
    const char *ip;

    #ifdef WIN32
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif

    g_loop = loop_new(1);
    assert(g_loop);
    ip = "127.0.0.1";
    if (argc >= 2)
    {
        ip = argv[1];
    }

    client1 = tcp_client_new(g_loop, ip, 16889, on_connected, on_data, on_close, NULL);
    assert(client1);
    tcp_client_connect(client1);

    loop_loop(g_loop);

    tcp_client_destroy(client1);
    loop_destroy(g_loop);

    #ifdef WIN32
    WSACleanup();
    #endif

    return 0;
}
