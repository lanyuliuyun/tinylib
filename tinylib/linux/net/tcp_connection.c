
#include "tinylib/linux/net/channel.h"
#include "tinylib/linux/net/tcp_connection.h"
#include "tinylib/linux/net/inetaddr.h"
#include "tinylib/linux/net/buffer.h"

#include "tinylib/util/log.h"

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/uio.h>
#include <sys/epoll.h>
#include <unistd.h>
#include <errno.h>

struct tcp_connection
{
    loop_t *loop;

    on_data_f datacb;
    on_close_f closecb;
    void* userdata;
    inetaddr_t peer_addr;
    inetaddr_t local_addr;

    int fd;
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
    int size;
};

static 
void delete_connection(tcp_connection_t *connection)
{
    log_debug("connection to %s:%u will be destroyed", connection->peer_addr.ip, connection->peer_addr.port);

    channel_detach(connection->channel);
    channel_destroy(connection->channel);
    shutdown(connection->fd, SHUT_RDWR);
    close(connection->fd);
    buffer_destory(connection->in_buffer);
    buffer_destory(connection->out_buffer);
    free(connection);

    return;
}

static 
void connection_onevent(int fd, int event, void* userdata)
{
    tcp_connection_t *connection = (tcp_connection_t*)userdata;
    inetaddr_t *peer_addr = &connection->peer_addr;
    
    buffer_t* in_buffer;
    buffer_t* out_buffer;
    void* data;
    int size;
    int written;
    int saved_errno;

    log_debug("connection_onevent: fd(%d), event(%d), peer addr: %s:%u", fd, event, peer_addr->ip, peer_addr->port);

    if (event & EPOLLHUP)
    {
        if (connection->need_closed_after_sent_done == 0)
        {
            connection->is_connected = 0;
            connection->is_in_callback = 1;
            connection->closecb(connection, connection->userdata);
            connection->is_in_callback = 0;
        }
        else
        {
            /* 至此，connection 已被执行 destroy 过，仅仅是为了尝试将余下的数据尽可能的发出去而被保留到现在
             * 但不幸的是，链接已经被远端断开了，此时只能放弃该链接，将其销毁
             */
            connection->is_alive = 0;
        }
    }
    else
    {
        if (event & EPOLLIN)
        {
            /* FIXME:每次响应可读事件，只执行一次 read 操作，有些浪费 poller 的通知，考虑循环读至读清 */
            if (connection->need_closed_after_sent_done == 0)
            {
                in_buffer = connection->in_buffer;
                size = buffer_readFd(in_buffer, connection->fd);
                if (size == 0)
                {
                    assert(NULL != connection->closecb);
                    connection->is_connected = 0;
                    connection->is_in_callback = 1;
                    connection->closecb(connection, connection->userdata);
                    connection->is_in_callback = 0;
                }
                else if (size > 0)
                {
                    assert(NULL != connection->datacb);
                    connection->is_in_callback = 1;
                    connection->datacb(connection, in_buffer, connection->userdata);
                    connection->is_in_callback = 0;
                }
            }
            else
            {
                char temp[256];
                while (read(fd, temp, sizeof(temp)) > 0);
            }
        }

        if (event & EPOLLOUT)
        {
            out_buffer = connection->out_buffer;
            data = buffer_peek(out_buffer);
            size = buffer_readablebytes(out_buffer);
            written = write(connection->fd, data, size);
            if (written < 0)
            {
                saved_errno = errno;
                if (saved_errno != EAGAIN && saved_errno != EINTR)
                {
                    log_error("connection_onevent: write() failed, fd(%d), errno(%d), peer addr: %s:%u", fd, saved_errno, peer_addr->ip, peer_addr->port);
                    return;
                }
                else
                {
                    written = 0;
                }
            }

            buffer_retrieve(out_buffer, written);

            if(written >= size)
            {
                channel_clearevent(connection->channel, EPOLLOUT);
                
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
    loop_t *loop, int fd, on_data_f datacb, on_close_f closecb, void* userdata, const inetaddr_t *peer_addr
)
{
    tcp_connection_t *connection;
    struct sockaddr_in addr;
    socklen_t addr_len;
    
    struct linger linger_info;
    int flag;

    if (NULL == loop || fd < 0 || NULL == datacb || NULL == closecb || NULL == peer_addr)
    {
        log_error("tcp_connection_new: bad loop(%p) or bad fd(%d) or bad datacb(%p) or bad closecb(%p) or bad peer_addr(%p)", 
            loop, fd, datacb, closecb, peer_addr);
        return NULL;
    }

    connection = (tcp_connection_t*)malloc(sizeof(*connection));
    memset(connection, 0, sizeof(*connection));

    flag = 1;
    setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, &flag, sizeof(flag));
    
    memset(&linger_info, 0, sizeof(linger_info));
    linger_info.l_onoff = 1;
    linger_info.l_linger = 3;
    setsockopt(fd, SOL_SOCKET, SO_LINGER, &linger_info, sizeof(linger_info));

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

    channel_setevent(connection->channel, EPOLLIN);

    return connection;
}

static
void do_tcp_connection_destroy(void *userdata)
{
    tcp_connection_t* connection = (tcp_connection_t*)userdata;

    if ((buffer_readablebytes(connection->out_buffer) > 0) && (connection->is_connected != 0))
    {
        channel_clearevent(connection->channel, EPOLLIN);
        channel_setevent(connection->channel, EPOLLOUT);

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

    return;
}

static 
int tcp_connection_sendInLoop(tcp_connection_t* connection, const void* data, int size)
{
    inetaddr_t *peer_addr = &connection->peer_addr;
    
    int fd;
    void* buffer_data;
    channel_t *channel;
    buffer_t* out_buffer;
    int written;
    unsigned buffer_left_data_size;
    struct iovec vecs[2];

    int error;

    out_buffer = connection->out_buffer;
    channel = connection->channel;
    fd = connection->fd;

    buffer_left_data_size = buffer_readablebytes(out_buffer);

    if (buffer_left_data_size > 0)
    {
        buffer_data = buffer_peek(out_buffer);
        vecs[0].iov_base = buffer_data;
        vecs[0].iov_len = buffer_left_data_size;
        vecs[1].iov_base = (void*)data;
        vecs[1].iov_len = size;
        
        written = writev(fd, vecs, 2);
        if (written < 0)
        {
            error = errno;
            
            if (error != EAGAIN && error != EINTR)
            {
                log_error("tcp_connection_sendInLoop: writev() failed, errno: %d, peer addr: %s:%u", errno, peer_addr->ip, peer_addr->port);
                return -1;
            }
            else
            {
                /* 尚未有数据提交发送OK，将本次新给的数据放入发送buffer，在后续 EPOLLOUT 事件中继续发送 */
                buffer_append(out_buffer, data, size);
                channel_setevent(channel, EPOLLOUT);
            }
        }
        else if (written == (buffer_left_data_size+size))
        {
            /* 当前所有的数据都发送完毕，一切安好则去除EPOLLOUT事件 */
            buffer_retrieveall(out_buffer);
            channel_clearevent(channel, EPOLLOUT);
        }
        else if (written < buffer_left_data_size)
        {
            /* out_buffer中的数据尚未发送完毕，则将本次的数据放入out_buffer后再发送 */
            buffer_retrieve(out_buffer, written);
            buffer_append(out_buffer, data, size);
            channel_setevent(channel, EPOLLOUT);
        }
        else if (written < (buffer_left_data_size+size))
        {
            /* out_buffer 中的数据发送完毕，但本次提交的数据部分未成功发送，将其放入 out_buffer 中，在后续 EPOLLOUT 事件中继续发送  */
            buffer_retrieveall(out_buffer);
            buffer_append(out_buffer, ((const char*)data+written-buffer_left_data_size), ((buffer_left_data_size+size)-written));
            channel_setevent(channel, EPOLLOUT);
        }
    }
    else
    {
        written = write(fd, data, size);
        if (written < 0)
        {
            error = errno;
            if (error != EAGAIN && error != EINTR)
            {
                log_error("tcp_connection_sendInLoop: write() failed, errno: %d, peer addr: %s:%u", errno, peer_addr->ip, peer_addr->port);
                return -1;
            }
            else
            {
                buffer_append(out_buffer, data, size);
                channel_setevent(channel, EPOLLOUT);
            }
        }
        else if (written < size)
        {
            buffer_append(out_buffer, ((const char*)data+written), (size-written));
            channel_setevent(channel, EPOLLOUT);
        }
        else
        {
            /* 当前所有的数据都发送完毕，一切安好则去除EPOLLOUT事件 */
            channel_clearevent(channel, EPOLLOUT);
        }
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
    
    if (connection == NULL || data == NULL || size <= 0)
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
    
    log_debug("tcp_connection_detach: fd(%d)", connection->fd);

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

    log_debug("tcp_connection_attach: fd(%d)", connection->fd);

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
    socklen_t len;

    if (NULL == connection)
    {
        return;
    }

    len = sizeof(send_buffer_size);
    result = getsockopt(connection->fd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, &len);
    if (0 != result)
    {
        log_error("tcp_connection_expand_send_buffer:　getsockopt() failed, errno: %d", errno);
        return;
    }

    send_buffer_size += ((size+1023)>>10)<<10;
    setsockopt(connection->fd, SOL_SOCKET, SO_SNDBUF, &send_buffer_size, sizeof(send_buffer_size));

    return;
}

void tcp_connection_expand_recv_buffer(tcp_connection_t *connection, unsigned size)
{
    int result;
    int recv_buffer_size;
    socklen_t len;

    if (NULL == connection)
    {
        return;
    }

    len = sizeof(recv_buffer_size);
    result = getsockopt(connection->fd, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, &len);
    if (0 != result)
    {
        log_error("tcp_connection_expand_recv_buffer:　getsockopt() failed, errno: %d", errno);
        return;
    }

    recv_buffer_size += ((size+1023)>>10)<<10;
    setsockopt(connection->fd, SOL_SOCKET, SO_RCVBUF, &recv_buffer_size, sizeof(recv_buffer_size));
    
    return;
}
