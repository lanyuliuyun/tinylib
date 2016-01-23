
#include "tinylib/linux/net/tcp_connection.h"
#include "tinylib/linux/net/tcp_client.h"
#include "tinylib/linux/net/channel.h"
#include "tinylib/linux/net/inetaddr.h"
#include "tinylib/linux/net/socket.h"

#include "tinylib/util/log.h"

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <unistd.h>

struct tcp_client
{
    loop_t *loop;
    inetaddr_t peer_addr;

    on_connected_f connectedcb;
    on_data_f datacb;
    on_close_f closecb;
    void* userdata;

    int fd;
    channel_t *channel;
    tcp_connection_t *connection;

	int is_in_callback;
	int is_alive;
};

static inline void delete_client(tcp_client_t* client)
{
	if (NULL != client)
	{
		channel_detach(client->channel);
		channel_destroy(client->channel);
		tcp_connection_destroy(client->connection);
		free(client);
	}

	return;
}

static void client_ondata(tcp_connection_t* connection, buffer_t* buffer, void* userdata)
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

static void client_onclose(tcp_connection_t* connection, void* userdata)
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

static void client_onevent(int fd, int event, void* userdata)
{
    tcp_client_t* client;
    tcp_connection_t *connection;

    client = (tcp_client_t*)userdata;
	
	channel_detach(client->channel);
	channel_destroy(client->channel);
	client->channel = NULL;
	
	log_debug("client_onevent: fd(%d), event(%d)", fd, event);

	if ((EPOLLERR | EPOLLHUP) & event)
	{
		log_error("failed to make connection to %s:%u, errno: %d", client->peer_addr.ip, client->peer_addr.port, errno);

		close(client->fd);
		client->fd = -1;
		
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
	
	if (EPOLLOUT & event)
	{
		log_debug("connection to %s:%u is ready", client->peer_addr.ip, client->peer_addr.port);

		connection  = tcp_connection_new(client->loop, fd, client_ondata, client_onclose, client, &client->peer_addr);
		if (NULL == connection)
		{
			log_error("can not create a connection object, connection to %s:%u aborts", client->peer_addr.ip, client->peer_addr.port);
			shutdown(client->fd, SHUT_RDWR);
			close(client->fd);
			client->fd = -1;

			client->is_in_callback = 1;
			client->connectedcb(NULL, client->userdata);
			client->is_in_callback = 0;

			if (0 == client->is_alive)
			{
				delete_client(client);
			}
			
			return;
		}
		
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

    if (NULL == loop || NULL == ip || 0 == port || NULL == datacb || NULL == closecb)
    {
        log_error("tcp_client_new: bad loop(%p) or bad ip(%p) or bad port(%u) or bad datacb(%p) or bad clsoecb(%p)",
                  loop, ip, port, datacb, closecb);
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

    client->fd = -1;
    client->channel = NULL;
    client->connection = NULL;

	client->is_in_callback = 0;
	client->is_alive = 1;
	
    return client;
}

int tcp_client_connect(tcp_client_t* client)
{
    int fd;
    int result;
    int err;
    struct sockaddr_in addr;

    if (NULL == client)
    {
        log_error("tcp_client_connect: bad client");
        return -1;
    }

    fd = create_client_socket();
    if (fd < 0)
    {
        log_error("tcp_client_new: create_client_socket() failed, peer addr: %s:%u", client->peer_addr.ip, client->peer_addr.port);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(client->peer_addr.port);
    addr.sin_addr.s_addr = inet_addr(client->peer_addr.ip);
    result = connect(fd, (struct sockaddr*)&addr, sizeof(addr));
    err = (result == 0) ? result : errno;

    switch(err)
    {
        case 0:
        case EINPROGRESS:
        case EINTR:
        case EISCONN:
        {
            client->fd = fd;
            break;
        }
        default:
        {
            close(fd);
            log_error("tcp_client_connect: connect() failed, peer addr: %s:%u, errno: %d", 
                client->peer_addr.ip, client->peer_addr.port, err);
            return -1;
        }
    }

    client->channel = channel_new(fd, client->loop, client_onevent, client);
    if (NULL == client->channel)
    {
		log_error("tcp_client_connect: channel_new() failed, failed to make connection to %s:%u", client->peer_addr.ip, client->peer_addr.port);
        close(fd);
        client->fd = -1;
        return -1;
    }
    channel_setevent(client->channel, EPOLLOUT);

    return 0;
}

tcp_connection_t* tcp_client_getconnection(tcp_client_t* client)
{
    return (NULL == client ? NULL : client->connection);
}

void tcp_client_destroy(tcp_client_t* client)
{
    if (NULL == client)
    {
        return;
    }

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

