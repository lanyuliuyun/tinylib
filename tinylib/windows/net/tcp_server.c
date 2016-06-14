
#include "tinylib/windows/net/tcp_server.h"
#include "tinylib/windows/net/socket.h"
#include "tinylib/windows/net/buffer.h"
#include "tinylib/windows/net/inetaddr.h"

#include "tinylib/util/log.h"

#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include <winsock2.h>

struct tcp_server
{
    loop_t* loop;
    SOCKET idle_fd;

    SOCKET fd;
    channel_t *channel;
    
    on_connection_f on_connection;
    void *userdata;

    inetaddr_t addr;

    int is_started;
    int is_in_callback;
    int is_alive;
};

static inline 
void delete_server(tcp_server_t *server)
{
    if (server->is_started)
    {
        tcp_server_stop(server);
    }

    free(server);

    return;
}

static 
void server_ondata(tcp_connection_t* connection, buffer_t* buffer, void* userdata)
{
    /* 默认读取数据之后不处理，直接丢弃 
     * 所以需要使用者在on_connection中自行将数据接受callback改为自己的处理函数
     */
    buffer_retrieveall(buffer);

    return;
}

static 
void server_onclose(tcp_connection_t* connection, void* userdata)
{
    /* 需要使用者在on_connection()中自行将close callback修改为自己的处理函数 */
    tcp_connection_destroy(connection);
    
    return;
}

static 
void server_onevent(SOCKET fd, short event, void* userdata)
{
    tcp_server_t *server = (tcp_server_t *)userdata;
    int error;
    
    tcp_connection_t* connection;
    SOCKET client_fd;
    struct sockaddr_in addr;
    int len;
    inetaddr_t peer_addr;

    log_debug("server_onevent: fd(%lu), event(%d), local addr(%s:%u)", fd, event, server->addr.ip, server->addr.port);
    
    len = sizeof(addr);
    memset(&addr, 0, len);
    client_fd = WSAAccept(fd, (struct sockaddr*)&addr, &len, NULL, 0);
    if (INVALID_SOCKET == client_fd)
    {
        error = WSAGetLastError();
        if (WSAEMFILE == error)
        {
            closesocket(server->idle_fd);
            client_fd = WSAAccept(fd, (struct sockaddr*)&addr, &len, NULL, 0);
            closesocket(client_fd);
            server->idle_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        }
        else
        {
            log_error("failed to accept a connection request, error: %d, local addr: %s:%u", error, server->addr.ip, server->addr.port);
        }

        return;
    }

    inetaddr_init(&peer_addr, &addr);

    log_debug("new connection arrived from %s:%d, local addr: %s:%u", 
        peer_addr.ip, peer_addr.port, server->addr.ip, server->addr.port);

    set_socket_onblock(client_fd, 1);
    connection = tcp_connection_new(server->loop, client_fd, server_ondata, server_onclose, server, &peer_addr);

    server->is_in_callback = 1;
    server->on_connection(connection, server->userdata, &peer_addr);
    server->is_in_callback = 0;
    
    if (0 == server->is_alive)
    {
        delete_server(server);
    }

    return;
}

tcp_server_t* tcp_server_new
(
    loop_t *loop, on_connection_f on_connection, void *userdata, unsigned short port, const char* ip
)
{
    tcp_server_t *server;
    
    if (NULL == loop || NULL == on_connection || NULL == ip || 0 == port)
    {
        log_error("tcp_server_new: bad loop(%p) or bad on_connection(%p) or bad ip(%p) or  bad port(%u)", loop, on_connection, ip, port);
        return NULL;
    }

    server = (tcp_server_t*)malloc(sizeof(tcp_server_t));
    memset(server, 0, sizeof(*server));
    
    server->loop = loop;
    server->idle_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    
    server->fd = -1;
    server->channel = NULL;
    server->on_connection = on_connection;
    server->userdata = userdata;
    inetaddr_initbyipport(&server->addr, ip, port);
    
    server->is_started = 0;
    server->is_in_callback = 0;
    server->is_alive = 1;

    return server;
}

void tcp_server_destroy(tcp_server_t *server)
{
    if (NULL == server)
    {
        return;
    }
    
    if (server->is_in_callback)
    {
        server->is_alive = 0;
    }
    else
    {
        delete_server(server);
    }

    return;
}

static
void do_tcp_server_start(void* userdata)
{
    tcp_server_t *server = (tcp_server_t*)userdata;

    do
    {
        server->fd = create_server_socket(server->addr.port, server->addr.ip);
        if (INVALID_SOCKET == server->fd)
        {
            log_error("do_tcp_server_start: create_server_socket() failed, local addr: %s:%u", server->addr.ip, server->addr.port);
            break;
        }

        server->channel = channel_new(server->fd, server->loop, server_onevent, server);

        if (listen(server->fd, 512) != 0)
        {
            log_error("do_tcp_server_start: listen() failed, errno: %d, local addr: %s:%u", WSAGetLastError(), server->addr.ip, server->addr.port);
            break;
        }

        channel_setevent(server->channel, POLLRDNORM);

        return;
    } while(0);

    if (INVALID_SOCKET != server->fd)
    {
        closesocket(server->fd);
        server->fd = INVALID_SOCKET;    
    }
    channel_destroy(server->channel);
    server->channel = NULL;
    server->is_started = 0;

    return;
}

int tcp_server_start(tcp_server_t *server)
{
    if (NULL == server)
    {
        log_error("tcp_server_start: bad server");
        return -1;
    }

    if (server->is_started)
    {
        return 0;
    }

    server->is_started = 1;
    loop_run_inloop(server->loop, do_tcp_server_start, server);

    return 0;
}

static
void do_tcp_server_stop(void* userdata)
{
    tcp_server_t *server = (tcp_server_t*)userdata;

    channel_detach(server->channel);
    channel_destroy(server->channel);
    server->channel = NULL;
    closesocket(server->idle_fd);
    server->idle_fd = INVALID_SOCKET;
    closesocket(server->fd);
    server->fd = INVALID_SOCKET;
    server->is_started = 0;

    return;
}

void tcp_server_stop(tcp_server_t *server)
{
    if (NULL == server)
    {
        return;
    }
    
    loop_run_inloop(server->loop, do_tcp_server_stop, server);
    
    return;
}
