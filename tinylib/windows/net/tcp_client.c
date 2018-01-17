
#include "tinylib/windows/net/tcp_client.h"
#include "tinylib/windows/net/channel.h"
#include "tinylib/windows/net/inetaddr.h"
#include "tinylib/windows/net/socket.h"

#include "tinylib/util/log.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include <winsock2.h>

struct tcp_client
{
    loop_t *loop;
    inetaddr_t peer_addr;

    on_connected_f connectedcb;
    on_data_f datacb;
    on_close_f closecb;
    void* userdata;

    SOCKET fd;
    channel_t *channel;
    tcp_connection_t *connection;
    
    int is_in_callback;
    int is_alive;
};

static inline 
void delete_client(tcp_client_t* client)
{
    channel_detach(client->channel);
    channel_destroy(client->channel);
    tcp_connection_destroy(client->connection);
    closesocket(client->fd);
    free(client);

    return;
}

static 
void client_ondata(tcp_connection_t* connection, buffer_t* buffer, void* userdata)
{
    tcp_client_t *client = (tcp_client_t *)userdata;

    client->is_in_callback = 1;
    client->datacb(connection, buffer, client->userdata);
    client->is_in_callback = 0;
    
    if (0 == client->is_alive)
    {
        delete_client(client);
    }

    return;
}

static 
void client_onclose(tcp_connection_t* connection, void* userdata)
{
    tcp_client_t *client = (tcp_client_t *)userdata;

    client->is_in_callback = 1;
    client->closecb(connection, client->userdata);
    client->is_in_callback = 0;

    if (0 == client->is_alive)
    {
        delete_client(client);
    }

    return;
}

static 
void client_onevent(SOCKET fd, int event, void* userdata)
{
    tcp_client_t* client;
    tcp_connection_t *connection;

    client = (tcp_client_t*)userdata;
    
    channel_detach(client->channel);
    channel_destroy(client->channel);
    client->channel = NULL;
    
    log_debug("client_onevent: fd(%lu), event(%d)", fd, event);

    if ((POLLERR | POLLNVAL) & event)
    {
        log_error("failed to make connection to %s:%u, error: %d", client->peer_addr.ip, client->peer_addr.port, WSAGetLastError());

        closesocket(client->fd);
        client->fd = INVALID_SOCKET;
        /* 通知用户连接失败了 */
        client->is_in_callback = 1;
        client->connectedcb(NULL, client->userdata);
        client->is_in_callback = 0;

        if (0 == client->is_alive)
        {
            delete_client(client);
        }

        return;
    }

    if (POLLOUT & event)
    {
        log_debug("connection to %s:%u is ready", client->peer_addr.ip, client->peer_addr.port);

        connection  = tcp_connection_new(client->loop, fd, client_ondata, client_onclose, client, &client->peer_addr);

        client->connection = connection;
        client->is_in_callback = 1;
        client->connectedcb(connection, client->userdata);
        client->is_in_callback = 0;

        if (0 == client->is_alive)
        {
            delete_client(client);
        }
    }

    return;
}

tcp_client_t* tcp_client_new
(
    loop_t *loop, const char* ip, unsigned short port, 
    on_connected_f connectedcb, on_data_f datacb, on_close_f closecb, void* userdata
)
{
    tcp_client_t *client;

    if (NULL == loop || NULL == ip || 0 == port || NULL == connectedcb || NULL == datacb || NULL == closecb)
    {
        log_error("tcp_client_new: bad loop(%p) or bad ip(%p) or bad port(%u) or bad connectedcb(%p) or bad datacb(%p) or bad clsoecb(%p)",
                  loop, ip, port, connectedcb, datacb, closecb);
        return NULL;
    }

    client = (tcp_client_t*)malloc(sizeof(tcp_client_t));
    memset(client, 0, sizeof(*client));
    
    client->loop = loop;
    inetaddr_initbyipport(&client->peer_addr, ip, port);

    client->connectedcb = connectedcb;
    client->datacb = datacb;
    client->closecb = closecb;
    client->userdata = userdata;

    client->fd = INVALID_SOCKET;
    client->channel = NULL;
    client->connection = NULL;

    client->is_in_callback = 0;
    client->is_alive = 1;

    return client;
}


static
void do_tcp_client_connect(void *userdata)
{
    tcp_client_t* client = (tcp_client_t*)userdata;

    SOCKET fd;
    int result;
    int error;
    struct sockaddr_in addr;

    fd = create_client_socket();
    if (INVALID_SOCKET == fd)
    {
        log_error("do_tcp_client_connect: create_client_socket() failed, dest addr: %s:%u, errno: %d", 
            client->peer_addr.ip, client->peer_addr.port, errno);

        client->is_in_callback = 1;
        client->connectedcb(NULL, client->userdata);
        client->is_in_callback = 0;

        if (0 == client->is_alive)
        {
            delete_client(client);
        }
        
        return;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client->peer_addr.port);
    addr.sin_addr.s_addr = inet_addr(client->peer_addr.ip);
    result = WSAConnect(fd, (struct sockaddr*)&addr, sizeof(addr), NULL, NULL, NULL, NULL);
    error = 0;
    if (SOCKET_ERROR == result)
    {
        error = WSAGetLastError();
    }

    if (0 == result || WSAEWOULDBLOCK == error)
    {
        client->channel = channel_new(fd, client->loop, client_onevent, client);
        client->fd = fd;
        channel_setevent(client->channel, POLLOUT);
    }
    else
    {
        log_error("tcp_client_connect: WSAConnect() failed, errno: %d, dest: %s:%u", error, client->peer_addr.ip, client->peer_addr.port);

        client->is_in_callback = 1;
        client->connectedcb(NULL, client->userdata);
        client->is_in_callback = 0;

        if (0 == client->is_alive)
        {
            delete_client(client);
        }
    }

    return;
}

int tcp_client_connect(tcp_client_t* client)
{
    if (NULL == client)
    {
        return -1;
    }

    loop_run_inloop(client->loop, do_tcp_client_connect, client);
    
    return 0;
}

tcp_connection_t* tcp_client_getconnection(tcp_client_t* client)
{
    return (NULL == client ? NULL : client->connection);
}

static
void do_tcp_client_destroy(void *userdata)
{
    tcp_client_t* client = (tcp_client_t*)userdata;

    if (client->is_in_callback)
    {
        client->is_alive = 0;
    }
    else
    {
        delete_client(client);
    }

    return;
}

void tcp_client_destroy(tcp_client_t* client)
{
    if (NULL == client)
    {
        return;
    }

    loop_run_inloop(client->loop, do_tcp_client_destroy, client);

    return;
}
