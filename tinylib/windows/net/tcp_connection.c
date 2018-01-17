

#include "tinylib/windows/net/channel.h"
#include "tinylib/windows/net/tcp_connection.h"
#include "tinylib/windows/net/inetaddr.h"
#include "tinylib/windows/net/buffer.h"

#include "tinylib/util/log.h"

#include <stdlib.h>
#include <assert.h>

struct tcp_connection
{
    loop_t *loop;

    on_data_f datacb;
    on_close_f closecb;
    void* userdata;
    inetaddr_t peer_addr;
    inetaddr_t local_addr;

    SOCKET fd;
    channel_t *channel;
    buffer_t *in_buffer;
    buffer_t *out_buffer;

    int is_in_callback;
    int is_alive;
    int is_connected;
    int need_closed_after_sent_done;
};

struct tcp_connection_msg
{
    tcp_connection_t* connection;
    void* data;
    unsigned size;
};

static 
void delete_connection(tcp_connection_t *connection)
{
    log_debug("connection to %s:%u will be destroyed", connection->peer_addr.ip, connection->peer_addr.port);

    channel_detach(connection->channel);
    channel_destroy(connection->channel);
    shutdown(connection->fd, SD_BOTH);
    closesocket(connection->fd);
    buffer_destory(connection->in_buffer);
    buffer_destory(connection->out_buffer);
    free(connection);
 
    return;
}

static 
void connection_onevent(SOCKET fd, int event, void* userdata)
{
    tcp_connection_t *connection = (tcp_connection_t*)userdata;
    inetaddr_t *peer_addr = &connection->peer_addr;

    buffer_t* in_buffer;
    buffer_t* out_buffer;
    int size;
    WSABUF wsabuf;
    unsigned total;
    DWORD written;
    int saved_error;

    log_debug("connection_onevent: fd(%lu), event(%d), peer addr: %s:%u", fd, event, peer_addr->ip, peer_addr->port);

    if (POLLHUP & event)
    {
        connection->is_connected = 0;
        connection->is_in_callback = 1;
        connection->closecb(connection, connection->userdata);
        connection->is_in_callback = 0;
    }
    else
    {
        if (POLLIN & event)
        {
            /* FIXME:每次响应可读事件，只执行一次 read 操作，有些浪费 poller 的通知，考虑循环读至读清 */
            in_buffer = connection->in_buffer;
            size = buffer_readFd(in_buffer, connection->fd);
            if (0 == size)
            {
                assert(NULL != connection->closecb);
                connection->is_connected = 0;
                connection->is_in_callback = 1;
                connection->closecb(connection, connection->userdata);
                connection->is_in_callback = 0;
            }
            else
            {
                assert(NULL != connection->datacb);
                connection->is_in_callback = 1;
                connection->datacb(connection, in_buffer, connection->userdata);
                connection->is_in_callback = 0;
            }
        }

        if (POLLOUT & event)
        {
            out_buffer = connection->out_buffer;
            assert(INVALID_SOCKET != connection->fd);

            memset(&wsabuf, 0, sizeof(wsabuf));
            total = buffer_readablebytes(out_buffer);
            wsabuf.len = total;
            wsabuf.buf = (char*)buffer_peek(out_buffer);
            written = 0;
            if (WSASend(connection->fd, &wsabuf, 1, &written, 0, NULL, NULL) != 0)
            {
                saved_error = WSAGetLastError();
                if (saved_error != WSAEWOULDBLOCK)
                {
                    log_error("connection_onevent: WSASend() failed, fd(%lu), errno(%d), peer addr: %s:%u", connection->fd, error, peer_addr->ip, peer_addr->port);
                }
                return;
            }
            buffer_retrieve(out_buffer, written);

            if (written >= total)
            {
                channel_clearevent(connection->channel, POLLOUT);

                if (connection->need_closed_after_sent_done)
                {
                    connection->is_alive = 0;
                }
            }
        }
    }

    if (0 == connection->is_alive)
    {
        delete_connection(connection);
    }

    return;
}

tcp_connection_t* tcp_connection_new
(
    loop_t *loop, SOCKET fd, on_data_f datacb, on_close_f closecb, void* userdata, const inetaddr_t *peer_addr
)
{
    tcp_connection_t *connection;
    struct sockaddr_in addr;
    int addr_len;

    struct linger linger_info;
    BOOL flag;
    
    if (NULL == loop || INVALID_SOCKET == fd || NULL == datacb || NULL == closecb || NULL == peer_addr)
    {
        log_error("tcp_connection_new: bad loop(%p) or bad fd or bad datacb(%p) or bad closecb(%p) or bad peer_addr(%p)", 
            loop, datacb, closecb, peer_addr);
        return NULL;
    }

    connection = (tcp_connection_t*)malloc(sizeof(*connection));
    memset(connection, 0, sizeof(*connection));

    flag = TRUE;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (char*)&flag, (int)sizeof(flag));

    memset(&linger_info, 0, sizeof(linger_info));
    linger_info.l_onoff = 1;
    linger_info.l_linger = 3;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, (char*)&linger_info, (int)sizeof(linger_info));

    connection->loop = loop;
    connection->fd = fd;
    connection->datacb = datacb;
    connection->closecb = closecb;
    connection->userdata = userdata;

    connection->channel = channel_new(fd, loop, connection_onevent, connection);

    connection->in_buffer = buffer_new(4096);
    connection->out_buffer = buffer_new(4096);

    connection->is_in_callback = 0;
    connection->is_alive = 1;
    connection->is_connected = 1;
    connection->need_closed_after_sent_done = 0;
    connection->peer_addr = *peer_addr;

    memset(&addr, 0, sizeof(addr));
    addr_len = sizeof(addr);
    getsockname(fd, (struct sockaddr*)&addr, &addr_len);
    inetaddr_init(&connection->local_addr, &addr);

    channel_setevent(connection->channel, POLLIN);

    return connection;
}

static
void do_tcp_connection_destroy(void *userdata)
{
    tcp_connection_t* connection = (tcp_connection_t*)userdata;

    if (buffer_readablebytes(connection->out_buffer) > 0)
    {
        channel_clearevent(connection->channel, POLLIN);
        channel_setevent(connection->channel, POLLOUT);

        connection->need_closed_after_sent_done = 1;
    }
    else
    {
        if (connection->is_in_callback)
        {
            connection->is_alive = 0;
        }
        else
        {
            delete_connection(connection);
        }
    }

    return;
}

void tcp_connection_destroy(tcp_connection_t* connection)
{
    if (NULL == connection)
    {
        return;
    }
    
    loop_run_inloop(connection->loop, do_tcp_connection_destroy, connection);
}

static 
int tcp_connection_sendInLoop(tcp_connection_t* connection, const void* data, int size)
{
    inetaddr_t *peer_addr = &connection->peer_addr;
    
    SOCKET fd;
    channel_t *channel;
    buffer_t* out_buffer;

    unsigned buffer_data_left;
    WSABUF wsabufs[2];
    DWORD bufcount;
    unsigned total;
    DWORD written;    
    int error;

    out_buffer = connection->out_buffer;
    channel = connection->channel;
    fd = connection->fd;

    buffer_data_left = buffer_readablebytes(out_buffer);

    memset(wsabufs, 0, sizeof(wsabufs));
    if (buffer_data_left > 0)
    {
        wsabufs[0].len = buffer_data_left;
        wsabufs[0].buf = buffer_peek(out_buffer);
        wsabufs[1].len = size;
        wsabufs[1].buf = (char*)data;
        bufcount = 2;
        total = buffer_data_left + size;
    }
    else
    {
        wsabufs[0].len = size;
        wsabufs[0].buf = (char*)data;
        bufcount = 1;
        total = size;
    }

    written = 0;
    if (WSASend(fd, wsabufs, bufcount, &written, 0, NULL, NULL) != 0)
    {
        error = WSAGetLastError();
        if (WSAENETRESET == error || WSAECONNRESET == error)
        {
            /* 底层的链接实际上已经断开了 */
            connection->is_connected = 0;
            connection->is_in_callback = 1;
            connection->closecb(connection, connection->userdata);
            connection->is_in_callback = 0;
        }
        else if (error != WSAEWOULDBLOCK)
        {
            log_error("tcp_connection_sendInLoop: WSASend() failed, errno: %d, peer addr: %s:%u", error, peer_addr->ip, peer_addr->port);
            return -1;
        }
    }

    if (written > 0 && written < total)
    {
        if (written < buffer_data_left)
        {
            buffer_retrieve(out_buffer, written);
            buffer_append(out_buffer, data, size);
        }
        else
        {
            buffer_retrieveall(out_buffer);
            buffer_append(out_buffer, ((char*)data + written - buffer_data_left), (size - (written - buffer_data_left)));
        }

        channel_setevent(channel, POLLOUT);
    }
    
    if (0 == connection->is_alive)
    {
        delete_connection(connection);
    }

    return 0;
}

static
void do_tcp_connection_send(void *userdata)
{
    struct tcp_connection_msg *connection_msg = (struct tcp_connection_msg *)userdata;
    
    tcp_connection_sendInLoop(connection_msg->connection, connection_msg->data, connection_msg->size);
    
    free(connection_msg);
    
    return;
}

int tcp_connection_send(tcp_connection_t* connection, const void* data, int size)
{
    struct tcp_connection_msg *connection_msg;
    
    if (NULL == connection || NULL == data || 0 >= size)
    {
        log_error("tcp_connection_send: bad connection(%p) or bad data(%p) or bad size(%u)", connection, data, size);
        return -1;
    }

    if (0 == connection->is_connected)
    {
        log_warn("not a opened connection");
        return -1;
    }

    if (loop_inloopthread(connection->loop))
    {
        tcp_connection_sendInLoop(connection, data, size);
    }
    else
    {
        connection_msg = (struct tcp_connection_msg *)malloc(sizeof(*connection_msg) + size);
        connection_msg->connection = connection;
        connection_msg->data = &connection_msg[1];
        connection_msg->size = size;
        memcpy(connection_msg->data, data, size);

        loop_async(connection->loop, do_tcp_connection_send, connection_msg);
    }

    return 0;
}

void tcp_connection_setcalback(tcp_connection_t* connection, on_data_f datacb, on_close_f closecb, void* userdata)
{
    if (NULL != connection)
    {
        connection->datacb = datacb;
        connection->closecb = closecb;
        connection->userdata = userdata;
    }

    return;
}

const inetaddr_t* tcp_connection_getpeeraddr(tcp_connection_t* connection)
{
    return NULL == connection ? NULL : &connection->peer_addr;
}

const inetaddr_t* tcp_connection_getlocaladdr(tcp_connection_t* connection)
{
    return NULL == connection ? NULL : &connection->local_addr;
}

void tcp_connection_detach(tcp_connection_t *connection)
{
    if (NULL == connection)
    {
        return;
    }

    log_debug("tcp_connection_detach: fd(%lu)", connection->fd);

    channel_detach(connection->channel);
    connection->loop = NULL;

    return;
}

static 
void do_connection_attach(void *userdata)
{
    tcp_connection_t *connection = (tcp_connection_t*)userdata;

    channel_attach(connection->channel, connection->loop);

    return;
}

void tcp_connection_attach(tcp_connection_t *connection, loop_t *loop)
{
    if (NULL == connection || NULL == loop)
    {
        log_error("tcp_connection_attach: bad connection(%p) or bad loop(%p)", connection, loop);
        return;
    }
    
    log_debug("tcp_connection_attach: fd(%lu)", connection->fd);

    connection->loop = loop;
    loop_run_inloop(loop, do_connection_attach, connection);

    return;
}

loop_t* tcp_connection_getloop(tcp_connection_t *connection)
{
    return NULL == connection ? NULL : connection->loop;
}

int tcp_connection_connected(tcp_connection_t *connection)
{
    return NULL == connection ? 0 : connection->is_connected;
}

void tcp_connection_expand_send_buffer(tcp_connection_t *connection, unsigned size)
{
    int result;
    int send_buffer_size;
    int len;
    
    if (NULL == connection)
    {
        return;
    }

    len = sizeof(send_buffer_size);
    result = getsockopt(connection->fd, SOL_SOCKET, SO_SNDBUF, (char*)&send_buffer_size, &len);
    if (0 != result)
    {
        log_error("tcp_connection_expand_send_buffer: getsockopt() failed, errno: %d", WSAGetLastError());
        return;
    }

    send_buffer_size += ((size+1023)>>10)<<10;
    setsockopt(connection->fd, SOL_SOCKET, SO_SNDBUF, (char*)&send_buffer_size, sizeof(send_buffer_size));

    return;
}

void tcp_connection_expand_recv_buffer(tcp_connection_t *connection, unsigned size)
{
    int result;
    int recv_buffer_size;
    int len;

    if (NULL == connection)
    {
        return;
    }

    len = sizeof(recv_buffer_size);
    result = getsockopt(connection->fd, SOL_SOCKET, SO_RCVBUF, (char*)&recv_buffer_size, &len);
    if (0 != result)
    {
        log_error("tcp_connection_expand_recv_buffer: getsockopt() failed, errno: %d", WSAGetLastError());
        return;
    }

    recv_buffer_size += ((size+1023)>>10)<<10;
    setsockopt(connection->fd, SOL_SOCKET, SO_RCVBUF, (char*)&recv_buffer_size, sizeof(recv_buffer_size));
    
    return;
}
