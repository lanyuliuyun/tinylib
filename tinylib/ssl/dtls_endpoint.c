
#include "tinylib/ssl/dtls_endpoint.h"
#include "tinylib/util/log.h"

#if defined(WIN32)
  #include "tinylib/windows/net/socket.h"
  #include <winsock2.h>

  #define errno WSAGetLastError()
#elif defined(__linux__)
  #include "tinylib/linux/net/socket.h"
  #include <unistd.h>
  #include <errno.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <sys/epoll.h>

  #define SOCKET int
  #define INVALID_SOCKET (-1)
  #define closesocket close

  #define POLLIN EPOLLIN
  #define POLLOUT EPOLLOUT
#endif

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/opensslv.h>

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

struct dtls_endoint
{
    loop_t *loop;
    SOCKET fd;
    channel_t *channel;
    
    dtls_endoint_on_handshanke_f handshakecb;
    dtls_endoint_on_shutdown_f shutdowncb;
    dtls_endoint_on_message_f messagecb;
    dtls_endoint_on_writable_f writecb;
    void *userdata;

    char local_ip[16];
    unsigned short local_port;
    char server_ip[16];
    unsigned short server_port;
    
    SSL_CTX *ssl_ctx;
    SSL *ssl;
    enum dtls_state dtls_state;
    unsigned char packet[65535];
    
    enum dtls_endpoint_mode endpoint_mode;
    
    int is_in_callback;
    int is_alive;
};

static
void delete_dtls_endoint(dtls_endoint_t *dtls_endoint, int is_passive_close)
{
    if (dtls_endoint->dtls_state == DTLS_STATE_SNDRCV && is_passive_close == 0)
    {
        SSL_shutdown(dtls_endoint->ssl);
    }
    SSL_free(dtls_endoint->ssl);
    SSL_CTX_free(dtls_endoint->ssl_ctx);

    channel_detach(dtls_endoint->channel);
    channel_destroy(dtls_endoint->channel);
    closesocket(dtls_endoint->fd);
    free(dtls_endoint);

    return;
}

static
void dtls_endoint_onevent(SOCKET fd, int event, void* userdata)
{
    dtls_endoint_t *dtls_endoint = (dtls_endoint_t*)userdata;
    int ssl_ret;
    int ssl_error;
    int is_passive_close = 0;
    
    assert(dtls_endoint->fd == fd);
    
    if (dtls_endoint->dtls_state == DTLS_STATE_HANDSHAKE)
    {
        ssl_ret = SSL_do_handshake(dtls_endoint->ssl);
        ssl_error = SSL_get_error(dtls_endoint->ssl, ssl_ret);
        if (ssl_ret == 1)
        {
            dtls_endoint->dtls_state = DTLS_STATE_SNDRCV;
            dtls_endoint->is_in_callback = 1;
            dtls_endoint->handshakecb(dtls_endoint, 1, dtls_endoint->userdata);
            dtls_endoint->is_in_callback = 0;
            
          #if 1
            {
                SRTP_PROTECTION_PROFILE *srtp_profile = SSL_get_selected_srtp_profile(dtls_endoint->ssl);
                if (srtp_profile)
                {
                    SSL_SESSION *ssl_sess = SSL_get_session(dtls_endoint->ssl);
                    printf("=== ssl master key: 0x");
                    for (int i = 0; i < ssl_sess->master_key_length; ++i)
                    {
                        printf("%X", ssl_sess->master_key[i]);
                    }
                    printf(" ===\n");
                    printf("=== selected srtp profile, id: %lu, name: %s ===\n", srtp_profile->id, srtp_profile->name);
                }
            }
          #endif
        }
        else if (ssl_error == SSL_ERROR_WANT_READ || ssl_error == SSL_ERROR_WANT_WRITE)
        {
            /* keep going and nothing todo */
        }
        else
        {
            dtls_endoint->is_in_callback = 1;
            dtls_endoint->handshakecb(dtls_endoint, 0, dtls_endoint->userdata);
            dtls_endoint->is_in_callback = 0;
        }
    }
    else if (dtls_endoint->dtls_state == DTLS_STATE_SNDRCV)
    {
        if (event & POLLIN)
        {
            ssl_ret = SSL_read(dtls_endoint->ssl, dtls_endoint->packet, sizeof(dtls_endoint->packet));
            if (ssl_ret > 0)
            {
                dtls_endoint->is_in_callback = 1;
                dtls_endoint->messagecb(dtls_endoint, dtls_endoint->packet, (unsigned)ssl_ret, dtls_endoint->userdata);
                dtls_endoint->is_in_callback = 0;
            }
            else if (ssl_ret == 0)
            {
                ssl_error = SSL_get_error(dtls_endoint->ssl, ssl_ret);
                if (ssl_error == SSL_ERROR_ZERO_RETURN)
                {
                    is_passive_close = 1;
                    dtls_endoint->is_in_callback = 1;
                    dtls_endoint->shutdowncb(dtls_endoint, 1, dtls_endoint->userdata);
                    dtls_endoint->is_in_callback = 0;
                }
                else
                {
                    log_error("dtls_endoint_onevent: ssl read error: %d, dtls_endoint: %p", ssl_error, dtls_endoint);
                    is_passive_close = 1;
                    dtls_endoint->is_in_callback = 1;
                    dtls_endoint->shutdowncb(dtls_endoint, 0, dtls_endoint->userdata);
                    dtls_endoint->is_in_callback = 0;
                }
            }
            else /* if (ssl_ret < 0) */
            {
                ssl_error = SSL_get_error(dtls_endoint->ssl, ssl_ret);
                log_error("dtls_endoint_onevent: ssl read error: %d, dtls_endoint: %p", ssl_error, dtls_endoint);

                /* TODO:  */
            }
        }
        if (event & POLLOUT)
        {
            if (dtls_endoint->writecb != 0)
            {
                dtls_endoint->is_in_callback = 1;
                dtls_endoint->writecb(dtls_endoint, dtls_endoint->userdata);
                dtls_endoint->is_in_callback = 0;
            }
        }
    }
    else
    {
        log_warn("dtls_endoint_onevent: unknown io event: %d, dtls_endoint: %p", event, dtls_endoint);
    }
    
    if (dtls_endoint->is_alive == 0)
    {
        delete_dtls_endoint(dtls_endoint, is_passive_close);
    }

    return;
}

static
void init_dtls_endoint_event(void *userdata)
{
    dtls_endoint_t *dtls_endoint = (dtls_endoint_t*)userdata;
    short event = POLLIN;
    if (dtls_endoint->writecb != NULL)
    {
        event |= POLLOUT;
    }
    channel_setevent(dtls_endoint->channel, event);

    return;
}

dtls_endoint_t* dtls_endoint_new(loop_t *loop, 
    const char *server_ip, unsigned short server_port, 
    const char *local_ip, unsigned short local_port, 
    dtls_endoint_on_handshanke_f handshakecb, dtls_endoint_on_shutdown_f shutdowncb,
    dtls_endoint_on_message_f messagecb, dtls_endoint_on_writable_f writecb, void *userdata,
    const char* ca_file, const char *private_key_file, const char *ca_pwd,
    enum dtls_endpoint_mode mode
)
{
    dtls_endoint_t *dtls_endoint;
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
        log_error("dtls_endoint_new: create_client_socket() failed, local addr: %s:%u, peer addr: %s:%u, errno: %d", 
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

    if (mode == DTLS_ENDPOINT_MODE_CLIENT)
    {
        ssl_ctx = SSL_CTX_new(DTLS_client_method());
    }
    else /* if (mode == DTLS_ENDPOINT_MODE_SERVER) */
    {
        ssl_ctx = SSL_CTX_new(DTLS_server_method());
    }
    
    if (ssl_ctx == NULL)
    {
        log_error("dtls_endoint_new: failed to alloc DTLSv1.2 ssl context, local addr: %s:%u, peer addr: %s:%u, errno: %d", 
            local_ip, local_port, server_ip, server_port, errno);
        closesocket(fd);
        return NULL;
    }
    if (SSL_CTX_use_certificate_file(ssl_ctx, ca_file, SSL_FILETYPE_PEM) != 1)
    {
        log_error("dtls_endoint_new: failed to load CA file, local addr: %s:%u, peer addr: %s:%u, errno: %d", 
            local_ip, local_port, server_ip, server_port, errno);
        SSL_CTX_free(ssl_ctx);
        closesocket(fd);
        return NULL;
    }
    SSL_CTX_set_default_passwd_cb_userdata(ssl_ctx, (char*)ca_pwd);
    
    if (private_key_file && SSL_CTX_use_PrivateKey_file(ssl_ctx, private_key_file, SSL_FILETYPE_PEM) != 1)
    {
        log_error("dtls_endoint_new: failed to load CA file, local addr: %s:%u, peer addr: %s:%u, errno: %d", 
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

    dtls_endoint = (dtls_endoint_t*)malloc(sizeof(*dtls_endoint));
    memset(dtls_endoint, 0, sizeof(*dtls_endoint));

    strncpy(dtls_endoint->server_ip, server_ip, sizeof(dtls_endoint->server_ip));
    dtls_endoint->server_port = server_port;
    strncpy(dtls_endoint->local_ip, (local_ip ? local_ip : "0.0.0.0"), sizeof(dtls_endoint->local_ip));
    dtls_endoint->local_port = local_port;
    
    dtls_endoint->handshakecb = handshakecb;
    dtls_endoint->shutdowncb = shutdowncb;
    dtls_endoint->messagecb = messagecb;
    dtls_endoint->writecb = writecb;
    dtls_endoint->userdata = userdata;

    dtls_endoint->ssl_ctx = ssl_ctx;
    dtls_endoint->ssl = ssl;
    dtls_endoint->dtls_state = DTLS_STATE_INIT;
    dtls_endoint->endpoint_mode = mode;

    dtls_endoint->loop = loop;
    dtls_endoint->fd = fd;
    dtls_endoint->channel = channel_new(fd, loop, dtls_endoint_onevent, dtls_endoint);
    dtls_endoint->is_alive = 1;
    dtls_endoint->is_in_callback = 0;
    
    loop_run_inloop(loop, init_dtls_endoint_event, dtls_endoint);

    return dtls_endoint;
}

int dtls_endoint_enable_srtp(dtls_endoint_t* dtls_endoint)
{
    int ssl_ret;

    /* TODO: 下列 SRTP profile 候选列表，需要根据 openssl 版本实际支持的结果做同步更新 */
    const char *srtp_profiles = "SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32";
  #if OPENSSL_VERSION_NUMBER >= 0x1010001fL
    srtp_profiles = "SRTP_AES128_CM_SHA1_80:SRTP_AES128_CM_SHA1_32:SRTP_AEAD_AES_128_GCM:SRTP_AEAD_AES_256_GCM";
  #endif

    ssl_ret  = SSL_set_tlsext_use_srtp(dtls_endoint->ssl, srtp_profiles);
    if (ssl_ret != 0)
    {
        printf("SSL_set_tlsext_use_srtp() failed, ret: %d\n", ssl_ret);
        return -1;
    }

    return 0;
}

static
void do_dtls_endoint_destroy(void *userdata)
{
    dtls_endoint_t *dtls_endoint = (dtls_endoint_t*)userdata;
    if (dtls_endoint->is_in_callback)
    {
        dtls_endoint->is_alive = 0;
    }
    else
    {
        delete_dtls_endoint(dtls_endoint, 0);
    }

    return;
}

void dtls_endoint_destroy(dtls_endoint_t* dtls_endoint)
{
    if (dtls_endoint == NULL)
    {
        return;
    }
    loop_run_inloop(dtls_endoint->loop, do_dtls_endoint_destroy, dtls_endoint);
    
    return;
}

static
void notify_handshake_ok(void *userdata)
{
    dtls_endoint_t *dtls_endoint = (dtls_endoint_t*)userdata;

    dtls_endoint->dtls_state = DTLS_STATE_SNDRCV;
    dtls_endoint->is_in_callback = 1;
    dtls_endoint->handshakecb(dtls_endoint, 1, dtls_endoint->userdata);
    dtls_endoint->is_in_callback = 0;

    return;
}

int dtls_endoint_start(dtls_endoint_t *dtls_endoint)
{
    int ssl_ret;
    int ssl_error;
    
    if (dtls_endoint == NULL)
    {
        return -1;
    }
    if (dtls_endoint->dtls_state != DTLS_STATE_INIT)
    {
        return -1;
    }

    dtls_endoint->dtls_state = DTLS_STATE_HANDSHAKE;
    if (dtls_endoint->endpoint_mode == DTLS_ENDPOINT_MODE_CLIENT)
    {
        SSL_set_connect_state(dtls_endoint->ssl);

        ssl_ret = SSL_do_handshake(dtls_endoint->ssl);
        ssl_error = SSL_get_error(dtls_endoint->ssl, ssl_ret);
        if (ssl_ret == 1)
        {
            loop_run_inloop(dtls_endoint->loop, notify_handshake_ok, dtls_endoint);
        }
        else if (ssl_error != SSL_ERROR_WANT_READ && ssl_error != SSL_ERROR_WANT_WRITE)
        {
            log_error("dtls_endoint_start: failed to start handshake, ssl ret: %d, ssl errno: %d", ssl_ret, ssl_error);
            return -1;
        }
    }
    else /* if (dtls_endoint->endpoint_mode == DTLS_ENDPOINT_MODE_SERVER) */
    {
        SSL_set_accept_state(dtls_endoint->ssl);
    }

    return 0;
}

int dtls_endoint_send(dtls_endoint_t* dtls_endoint, const void *packet, unsigned size)
{
    int ssl_ret;

    if (dtls_endoint == NULL || packet == NULL || size == 0)
    {
        log_error("dtls_endoint_send: bad args, dtls_endoint: %p, packet: %p, size: %u", dtls_endoint, packet, size);
        return -1;
    }
    
    if (dtls_endoint->dtls_state != DTLS_STATE_SNDRCV)
    {
        log_error("dtls_endoint_send: bad dtls state: %d, dtls_endoint: %p", dtls_endoint->dtls_state, dtls_endoint);
        return -1;
    }

    ssl_ret = SSL_write(dtls_endoint->ssl, packet, size);
    if (ssl_ret <= 0)
    {
        int ssl_err  = SSL_get_error(dtls_endoint->ssl, ssl_ret);
        log_warn("dtls_endoint_send: faild to send out packet, ssl_error: %d, dtls_endoint: %p", ssl_err, dtls_endoint);
        return -1;
    }

    return 0;
}
