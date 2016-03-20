
#include "tinylib/rtsp/rtsp_server.h"

#ifdef WINNT
    #include "tinylib/windows/net/tcp_server.h"
#elif defined(__linux__)
    #include "tinylib/linux/net/tcp_server.h"
#endif

#include "tinylib/util/log.h"

#include <string.h>
#include <stdlib.h>

struct rtsp_server
{
    tcp_server_t *server;
    rtsp_session_handler_f session_handler;
    rtsp_session_interleaved_packet_f interleaved_sink;
    void* userdata;
    
    char ip[16];
    unsigned short port;

    int started;
    int is_in_callback;
    int is_alive;
};

static inline 
void delete_server(rtsp_server_t *server)
{
    tcp_server_destroy(server->server);
    free(server);
    
    return;
}

static void 
server_onconnection(tcp_connection_t* connection, void* userdata, const inetaddr_t* peer_addr)
{
    rtsp_server_t *server;
    rtsp_session_t *session;

    if (NULL == connection || NULL == userdata)
    {
        return;
    }

    (void)peer_addr;
    
    server = (rtsp_server_t *)userdata;
    session = rtsp_session_start(connection, server->session_handler, server->interleaved_sink, server->userdata);
    
    (void)session;

    /* FIXME: rtsp_server 内部对 session 进行登记管理?  */

    return;
}

rtsp_server_t* rtsp_server_new
(
    loop_t *loop, 
    rtsp_session_handler_f session_handler, 
    rtsp_session_interleaved_packet_f interleaved_sink, 
    void *userdata,
    unsigned short port, 
    const char* ip
)
{
    rtsp_server_t *server;

    if (NULL == loop || NULL == session_handler || NULL == interleaved_sink ||  0 == port || NULL == ip)
    {
        log_error("rtsp_server_new: bad loop(%p) or bad session_handler(%p) or interleaved_sink(%p) bad  or bad port(%u) or bad ip(%p)", 
            loop, session_handler, interleaved_sink, port, ip);
        return NULL;
    }

    server = (rtsp_server_t*)malloc(sizeof(rtsp_server_t));
    memset(server, 0, sizeof(*server));

    server->session_handler = session_handler;
    server->interleaved_sink = interleaved_sink;
    server->userdata = userdata;
    strncpy(server->ip, ip, sizeof(server->ip)-1);
    server->port = port;

    server->server = tcp_server_new(loop, server_onconnection, server, port, ip);
    if (NULL == server->server)
    {
        free(server);
        log_error("rtsp_server_new: tcp_server_new() failed");
        return NULL;
    }
    
    server->started = 0;
    server->is_in_callback = 0;
    server->is_alive = 1;

    return server;
}

int rtsp_server_start(rtsp_server_t *server)
{
    if (NULL == server)
    {
        log_error("rtsp_server_start: bad server");
        return -1;
    }

    if (server->started)
    {
        log_warn("rtsp_server_start: server has already been started");
        return 0;
    }

    if (tcp_server_start(server->server) != 0)
    {
        log_error("rtsp_server_start: failed to start tcp server at %s:%u", 
            server->ip, server->port);
        return -1;
    }

    server->started = 1;

    return 0;
}

void rtsp_server_stop(rtsp_server_t *server)
{
    if (NULL == server)
    {
        return;
    }

    tcp_server_stop(server->server);
    server->started = 0;

    return;
}

void rtsp_server_destroy(rtsp_server_t *server)
{
    if (NULL == server)
    {
        return;
    }

    if (server->started)
    {
        rtsp_server_stop(server);
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

