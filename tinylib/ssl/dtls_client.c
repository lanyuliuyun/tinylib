
#include "tinylib/ssl/dtls_client.h"
#include "tinylib/util/log.h"

#ifdef WIN32
  #include "tinylib/windows/net/socket.h"
  #include <winsock2.h>

  #define errno WSAGetLastError()
#else
  #include "tinylib/linux/net/socket.h"
  #include <unistd.h>
  #include <errno.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <sys/epoll.h>
  
  typedef int SOCKET;
  #define INVALID_SOCKET (-1)
  #define closesocket close
  
  #define POLLIN EPOLLIN
  #define POLLOUT EPOLLOUT
#endif

#include <openssl/ssl.h>

#include <stdlib.h>
#include <string.h>
#include <assert.h>

enum dtls_state
{
    DTLS_STATE_INIT,
    DTLS_STATE_HANDSHAKE,
    DTLS_STATE_SNDRCV,
    DTLS_STATE_SHUTDOWN,
};

struct dtls_client
{
    loop_t *loop;
  #ifdef WIN32
    SOCKET fd;
  #else
    int fd;
  #endif
    channel_t *channel;
    
    dtls_client_on_handshanke_f handshakecb;
    dtls_client_on_shutdown_f shutdowncb;
    dtls_client_on_message_f messagecb;
    dtls_client_on_writable_f writecb;
    void *userdata;

    char local_ip[16];
    unsigned short local_port;
    char server_ip[16];
    unsigned short server_port;
    
    SSL_CTX *ssl_ctx;
    SSL *ssl;
    enum dtls_state dtls_state;
    unsigned char packet[65535];
};

static
void dtls_client_onevent(SOCKET fd, int event, void* userdata)
{
    dtls_client_t *dtls_client = (dtls_client_t*)userdata;
    int ssl_ret;
    int ssl_error;
    
    assert(dtls_client->fd == fd);
    
    if (dtls_client->dtls_state == DTLS_STATE_HANDSHAKE)
    {
        ssl_ret = SSL_do_handshake(dtls_client->ssl);
        ssl_error = SSL_get_error(dtls_client->ssl, ssl_ret);
        if (ssl_ret == 1)
        {
            dtls_client->dtls_state = DTLS_STATE_SNDRCV;
            dtls_client->handshakecb(dtls_client, 1, dtls_client->userdata);
        }
        else if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
        {
            /* keep going and nothing todo */
        }
        else
        {
            dtls_client->handshakecb(dtls_client, 0, dtls_client->userdata);
        }
    }
    else if (dtls_client->dtls_state == DTLS_STATE_SNDRCV)
    {
        if (event & POLLIN)
        {
            ssl_ret = SSL_read(dtls_client->ssl, dtls_client->packet, sizeof(dtls_client->packet));
            if (ssl_ret > 0)
            {
                dtls_client->messagecb(dtls_client, dtls_client->packet, (unsigned)ssl_ret, dtls_client->userdata);
            }
            else if (ssl_ret == 0)
            {
                ssl_error = SSL_get_error(dtls_client->ssl, ssl_ret);
                if (ssl_error == SSL_ERROR_ZERO_RETURN)
                {
                    dtls_client->shutdowncb(dtls_client, 1, dtls_client->userdata);
                }
                else
                {
                    log_error("dtls_client_onevent: ssl read error: %d, dtls_client: %p", ssl_error, dtls_client);
                    dtls_client->shutdowncb(dtls_client, 0, dtls_client->userdata);
                }
            }
            else /* if (ssl_ret < 0) */
            {
                ssl_error = SSL_get_error(dtls_client->ssl, ssl_ret);
                log_error("dtls_client_onevent: ssl read error: %d, dtls_client: %p", ssl_error, dtls_client);

                /* TODO:  */
            }
        }
        if (event & POLLOUT)
        {
            if (dtls_client->writecb != 0)
            {
                dtls_client->writecb(dtls_client, dtls_client->userdata);
            }
        }
    }
    else
    {
        log_warn("dtls_client_onevent: unknown io event: %d, dtls_client: %p", event, dtls_client);
    }

    return;
}

static
void init_dtls_client_event(void *userdata)
{
    dtls_client_t *dtls_client = (dtls_client_t*)userdata;
    short event = POLLIN;
    if (dtls_client->writecb != NULL)
    {
        event |= POLLOUT;
    }

    channel_setevent(dtls_client->channel, event);

    return;
}

dtls_client_t* dtls_client_new(loop_t *loop, 
    const char *server_ip, unsigned short server_port, 
    const char *local_ip, unsigned short local_port, 
    dtls_client_on_handshanke_f handshakecb, dtls_client_on_shutdown_f shutdowncb,
    dtls_client_on_message_f messagecb, dtls_client_on_writable_f writecb, void *userdata,
    const char* ca_file, const char *private_key_file, const char *ca_pwd
)
{
    dtls_client_t *dtls_client;
    SOCKET fd;
    struct sockaddr_in dest_addr;
    int buffer_size;
  #ifdef WIN32
    int len;
  #else
    socklen_t len;
  #endif
    
    SSL_CTX *ssl_ctx;
    SSL *ssl;

    if (loop == NULL || server_ip == NULL  || server_port == 0 || messagecb == NULL || 
        ca_file == NULL || ca_file == NULL || ca_pwd == NULL)
    {
        return NULL;
    }

    fd = create_udp_socket(local_port, local_ip);
    if (fd == INVALID_SOCKET)
    {
        log_error("dtls_client_new: create_client_socket() failed, local addr: %s:%u, peer addr: %s:%u, errno: %d", 
            local_ip, local_port, server_ip, server_port, errno);
        return NULL;
    }
    len = sizeof(buffer_size);
    if (getsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, &len) == 0)
    {
        buffer_size += 4096;
        setsockopt(fd, SOL_SOCKET, SO_SNDBUF, (char*)&buffer_size, sizeof(buffer_size));
    }
    len = sizeof(buffer_size);
    if (getsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, &len) == 0)
    {
        buffer_size += 4096;
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, (char*)&buffer_size, sizeof(buffer_size));
    }

    dest_addr.sin_family = AF_INET;
    dest_addr.sin_addr.s_addr = inet_addr(server_ip);
    dest_addr.sin_port = htons(server_port);
    connect(fd, (struct sockaddr*)&dest_addr, sizeof(dest_addr));

    /* ssl_ctx = SSL_CTX_new(DTLS_client_method()); */
    /* ssl_ctx = SSL_CTX_new(DTLS_client_method()); */
    ssl_ctx = SSL_CTX_new(DTLSv1_2_client_method());
    if (ssl_ctx == NULL)
    {
        log_error("dtls_client_new: failed to alloc DTLSv1.2 ssl context, local addr: %s:%u, peer addr: %s:%u, errno: %d", 
            local_ip, local_port, server_ip, server_port, errno);
        closesocket(fd);
        return NULL;
    }
    if (SSL_CTX_use_certificate_file(ssl_ctx, ca_file, SSL_FILETYPE_PEM) != 1)
    {
        log_error("dtls_client_new: failed to load CA file, local addr: %s:%u, peer addr: %s:%u, errno: %d", 
            local_ip, local_port, server_ip, server_port, errno);
        SSL_CTX_free(ssl_ctx);
        closesocket(fd);
        return NULL;
    }
    SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, (char*)ca_pwd);
    
    if (private_key_file && SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key_file, SSL_FILETYPE_PEM) != 1)
    {
        log_error("dtls_client_new: failed to load CA file, local addr: %s:%u, peer addr: %s:%u, errno: %d", 
            local_ip, local_port, server_ip, server_port, errno);
        SSL_CTX_free(ssl_ctx);
        closesocket(fd);
        return NULL;
    }

    ssl = SSL_new(ssl_ctx);
    if (ssl == NULL)
    {
        SSL_CTX_free(ssl_ctx);
        closesocket(fd);
        return NULL;
    }
    SSL_set_fd(ssl, fd);
    
    /* more ssl options can be placed here */
    
    dtls_client = (dtls_client_t*)malloc(sizeof(*dtls_client));
    memset(dtls_client, 0, sizeof(*dtls_client));

    strncpy(dtls_client->server_ip, server_ip, sizeof(dtls_client->server_ip));
    dtls_client->server_port = server_port;
    strncpy(dtls_client->local_ip, (local_ip ? local_ip : "0.0.0.0"), sizeof(dtls_client->local_ip));
    dtls_client->local_port = local_port;
    
    dtls_client->handshakecb = handshakecb;
    dtls_client->messagecb = messagecb;
    dtls_client->writecb = writecb;
    dtls_client->userdata = userdata;

    dtls_client->ssl_ctx = ssl_ctx;
    dtls_client->ssl = ssl;
    dtls_client->dtls_state = DTLS_STATE_INIT;

    dtls_client->loop = loop;
    dtls_client->fd = fd;
    dtls_client->channel = channel_new(fd, loop, dtls_client_onevent, dtls_client);
    loop_run_inloop(loop, init_dtls_client_event, dtls_client);

    return dtls_client;
}

static
void do_dtls_client_destroy(void *userdata)
{
    dtls_client_t *dtls_client = (dtls_client_t*)userdata;

    SSL_shutdown(dtls_client->ssl);
    SSL_free(dtls_client->ssl);
    SSL_CTX_free(dtls_client->ssl_ctx);
    closesocket(dtls_client->fd);
    free(dtls_client);

    return;
}

void dtls_client_destroy(dtls_client_t* dtls_client)
{
    if (dtls_client == NULL)
    {
        return;
    }
    loop_run_inloop(dtls_client->loop, do_dtls_client_destroy, dtls_client);
    
    return;
}

static
void notify_handshake_ok(void *userdata)
{
    dtls_client_t *dtls_client = (dtls_client_t*)userdata;

    dtls_client->dtls_state = DTLS_STATE_SNDRCV;
    dtls_client->handshakecb(dtls_client, 1, dtls_client->userdata);

    return;
}

int dtls_client_start(dtls_client_t *dtls_client)
{
    int ssl_ret;
    int ssl_error;
    
    if (dtls_client == NULL)
    {
        return -1;
    }
    if (dtls_client->dtls_state != DTLS_STATE_INIT)
    {
        return -1;
    }

    dtls_client->dtls_state = DTLS_STATE_HANDSHAKE;
    SSL_set_connect_state(dtls_client->ssl);
    ssl_ret = SSL_do_handshake(dtls_client->ssl);
    ssl_error = SSL_get_error(dtls_client->ssl, ssl_ret);
    if (ssl_ret == 1)
    {
        loop_run_inloop(dtls_client->loop, notify_handshake_ok, dtls_client);
    }
    else if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE)
    {
        log_error("dtls_client_start: failed to start handshake, ssl ret: %d, ssl errno: %d", ssl_ret, ssl_error);
        return -1;
    }

    return 0;
}

int dtls_client_send(dtls_client_t* dtls_client, void *packet, unsigned size)
{
    int ssl_ret;
    
    if (dtls_client == NULL || packet == NULL || size == 0)
    {
        log_error("dtls_client_send: bad args, dtls_client: %p, packet: %p, size: %u", dtls_client, packet, size);
        return -1;
    }
    
    if (dtls_client->dtls_state != DTLS_STATE_SNDRCV)
    {
        log_error("dtls_client_send: bad dtls state: %d, dtls_client: %p", dtls_client->dtls_state, dtls_client);
        return -1;
    }
    
    ssl_ret = SSL_write(dtls_client->ssl, packet, size);
    if (ssl_ret <= 0)
    {
        int ssl_err  = SSL_get_error(dtls_client->ssl, ssl_ret);
        log_warn("dtls_client_send: faild to send out packet, ssl_error: %d, dtls_client: %p", ssl_err, dtls_client);
        return -1;
    }

    return 0;
}
