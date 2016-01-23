

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
};

static void delete_connection(tcp_connection_t *connection)
{
	if (NULL != connection)
	{
		log_debug("connection to %s:%u will be destroyed", connection->peer_addr.ip, connection->peer_addr.port);

		channel_detach(connection->channel);
		channel_destroy(connection->channel);
		shutdown(connection->fd, SD_BOTH);
		closesocket(connection->fd);
		buffer_destory(connection->in_buffer);
		buffer_destory(connection->out_buffer);
		free(connection);
	}

    return;
}

static void connection_onevent(SOCKET fd, short event, void* userdata)
{
    tcp_connection_t *connection;
    buffer_t* in_buffer;
    buffer_t* out_buffer;
    unsigned size;
    WSABUF wsabuf;
    unsigned total;
    DWORD written;
	int error;

    log_debug("connection_onevent: fd(%lu), event(%d)", fd, event);

    connection = (tcp_connection_t*)userdata;

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
                error = WSAGetLastError();
				if (error != WSAEWOULDBLOCK)
				{
					log_error("connection_onevent: WSASend() failed, fd(%lu), errno(%d)", connection->fd, error);
					return;
				}
				else
				{
					written = 0;
				}
            }

			if(written == total)
			{
				channel_clearevent(connection->channel, POLLOUT);
			}
			buffer_retrieve(out_buffer, written);
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

    connection = (tcp_connection_t*)malloc(sizeof(tcp_connection_t));

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
    if (NULL == connection->channel)
    {
        log_error("tcp_connection_new: channel_new() failed");
        free(connection);
        return NULL;
    }

    connection->in_buffer = buffer_new(4096);
    if (NULL == connection->in_buffer)
    {
        log_error("tcp_connection_new: buffer_new() failed");
        channel_destroy(connection->channel);
        free(connection);
        return NULL;
    }

    connection->out_buffer = buffer_new(4096);
    if (NULL == connection->out_buffer)
    {
        log_error("tcp_connection_new: buffer_new() failed");
        buffer_destory(connection->in_buffer);
        channel_destroy(connection->channel);        
        free(connection);
        return NULL;
    }

    connection->is_in_callback = 0;
    connection->is_alive = 1;
    connection->is_connected = 1;
    connection->peer_addr = *peer_addr;

    memset(&addr, 0, sizeof(addr));
    addr_len = sizeof(addr);
    getsockname(fd, (struct sockaddr*)&addr, &addr_len);
    inetaddr_init(&connection->local_addr, &addr);

    channel_setevent(connection->channel, POLLIN);

    return connection;
}

void tcp_connection_destroy(tcp_connection_t* connection)
{
    if (NULL == connection)
    {
        return;
    }

    if (connection->is_in_callback)
    {
        connection->is_alive = 0;
    }
    else
    {
        delete_connection(connection);
    }

    return;
}

int tcp_connection_send(tcp_connection_t* connection, const void* data, unsigned size)
{
    SOCKET fd;
    channel_t *channel;
    buffer_t* out_buffer;

    unsigned buffer_data_left;
    WSABUF wsabufs[2];
    DWORD bufcount;
    unsigned total;
    DWORD written;    
    int error;

    if (NULL == connection || NULL == data || 0 == size)
    {
        log_error("tcp_connection_send: bad connection(%p) or bad data(%p) or bad size(%u)", connection, data, size);
        return -1;
    }

	if (0 == connection->is_connected)
	{
		log_warn("connection to %s:%u has NOT beed made yet", connection->peer_addr.ip, connection->peer_addr.port);
		return 0;
	}

    out_buffer = connection->out_buffer;
    channel = connection->channel;
    assert(NULL != out_buffer);
    assert(NULL != channel);
    fd = channel_getfd(channel);

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
		if (error != WSAEWOULDBLOCK)
		{
			log_error("tcp_connection_send: WSASend() failed, fd(%lu), errno(%d)", fd, error);
			return -1;
		}
    }

    if (written < total)
    {
        if (written < buffer_data_left)
        {
            /* out_buffer中的数据尚未发送完毕，则将本次的数据放入out_buffer后下次WRITE事件中再发送 */
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

static void do_connection_attach(void *userdata)
{
    tcp_connection_t *connection = (tcp_connection_t*)userdata;
    if (NULL != connection)
    {
        channel_attach(connection->channel, connection->loop);
    }
    else
    {
        log_error("do_connection_attach: bad connection");
    }

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
