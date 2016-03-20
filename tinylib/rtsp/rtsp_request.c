

#include "tinylib/rtsp/rtsp_request.h"
#include "tinylib/rtsp/rtsp_message_codec.h"
#include "tinylib/util/url.h"
#include "tinylib/util/log.h"

#ifdef WINNT
    #include "tinylib/windows/net/tcp_client.h"
    #include "tinylib/windows/net/tcp_connection.h"
    
    #include <winsock2.h>
#elif defined(__linux__)
    #include "tinylib/linux/net/tcp_client.h"
    #include "tinylib/linux/net/tcp_connection.h"
    
    #include <arpa/inet.h>
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

struct rtsp_request
{
    tcp_client_t *client;           /* 本请求对象使用的tcp client */
    loop_t* loop;

    char *url;    
    url_t *u;
    char pure_url[1024];           /* 不含user/password的url */

    rtsp_request_handler_f request_handler; /* user提供的请求响应处理函数 */
    rtsp_request_interleaved_packet_f interleaved_sink;
    void* userdata;
    
    rtsp_response_msg_t *response_msg;
    
    int is_new_response;                /* 标记一则新的rtsp客户端消息的开始，可能是新的rtsp请求消息，或interleaved消息
                                     * 在最开始和每处理完一则消息，需要将其置为1
                                     */

    int is_interleaved_message;        /* 标记是否是一则interleaved消息 */

    int cseq;                       /* 当前正在执行的请求的cseq */
    rtsp_method_e current_request;            /* 当前正在执行的请求 */

    int server_methods;             /* 远端rtsp服务器所支持的方法 */

    int got_sessionid;
    char *sessionid;                   /* 在setup请求之后服务器返回的Session ID */
    int timeout;                    /* request timeout seconds */

    int need_auth;                  /* 服务器告知是否需要认证 */
    
    int is_in_handler;              /* 标记当前请求对象是否在相应回调函数的执行序列中 */
    int is_alive;                   /* 标记当前请求对象是否是活动的，若不是活动的，则在回调执行完毕之后会将其删除 */
};

static inline
void request_delete(rtsp_request_t *request)
{
    tcp_client_destroy(request->client);
    free(request->sessionid);
    url_release(request->u);
    free(request->url);
    free(request);

    return;
}

static 
void request_filt_response
(
    rtsp_request_t *request, 
    tcp_connection_t* connection, 
    rtsp_method_e method, 
    rtsp_response_msg_t *response_msg
)
{
    char* pos;
    rtsp_head_t *head = NULL;
    
    if (RTSP_METHOD_OPTIONS == method)
    {
        head = response_msg->head;
        while (NULL != head)
        {
            if (RTSP_HEAD_PUBLIC == head->key)
            {
                break;
            }
            
            head = head->next;
        }

        if (NULL != head)
        {
            if (strstr(head->value, "OPTIONS"))
            {
                request->server_methods |= RTSP_METHOD_OPTIONS;
            }
            if (strstr(head->value, "DESCRIBE"))
            {
                request->server_methods |= RTSP_METHOD_DESCRIBE;
            }
            if (strstr(head->value, "ANNOUNCE"))
            {
                request->server_methods |= RTSP_METHOD_ANNOUNCE;
            }                
            if (strstr(head->value, "SETUP"))
            {
                request->server_methods |= RTSP_METHOD_SETUP;
            }
            if (strstr(head->value, "PLAY"))
            {
                request->server_methods |= RTSP_METHOD_PLAY;
            }
            if (strstr(head->value, "PAUSE"))
            {
                request->server_methods |= RTSP_METHOD_PAUSE;
            }
            if (strstr(head->value, "TEARDOWN"))
            {
                request->server_methods |= RTSP_METHOD_TEARDOWN;
            }
            if (strstr(head->value, "GET_PARAMETER"))
            {
                request->server_methods |= RTSP_METHOD_GET_PARAMETER;
            }
            if (strstr(head->value, "SET_PARAMETER"))
            {
                request->server_methods |= RTSP_METHOD_SET_PARAMETER;
            }
            if (strstr(head->value, "REDIRECT"))
            {
                request->server_methods |= RTSP_METHOD_REDIRECT;
            }
            if (strstr(head->value, "RECORD"))
            {
                request->server_methods |= RTSP_METHOD_RECORD;
            }
        }
    }
    else if (RTSP_METHOD_SETUP == method)
    {
        head = response_msg->head;
        while(NULL != head)
        {
            if (head->key == RTSP_HEAD_SESSION)
            {
                request->timeout = 0;
                free(request->sessionid);
                request->sessionid = NULL;
                
                pos = strchr(head->value, ';');
                if (NULL == pos)
                {
                    request->sessionid = strdup(head->value);
                }
                else
                {
                    *pos = '\0';
                    request->sessionid = strdup(head->value);
                    
                    pos++;
                    while(*pos == ' ' || *pos == '\t')++pos;
                    pos = strchr(pos, '=');
                    if (NULL != pos)
                    {
                        pos++;
                        request->timeout = atoi(pos);
                    }
                }

                request->got_sessionid = 1;

                break;
            }

            head = head->next;
        }
    }

    return;
}

static 
void request_ondata(tcp_connection_t* connection, buffer_t* buffer, void* userdata)
{
    rtsp_request_t *request;

    unsigned parsed_bytes;
    char* data;
    unsigned size;
    char ch;

    uint16_t interleaved_len;
    int ret;

    request = (rtsp_request_t *)userdata;

    data = (char*)buffer_peek(buffer);
    size = buffer_readablebytes(buffer);
    if (request->is_new_response)
    {
        while (1)
        {
            data = (char*)buffer_peek(buffer);
            size = buffer_readablebytes(buffer);

            ch = data[0];
            if (ch == 0x24)
            {
                request->is_new_response = 0;
                request->is_interleaved_message = 1;

                ch = data[1];
                interleaved_len = ntohs(*(uint16_t*)&data[2]);
                if (size < (unsigned)(interleaved_len+4))
                {
                    return;
                }

                request->interleaved_sink(request, (unsigned char)ch, &data[4], interleaved_len, request->userdata);
                buffer_retrieve(buffer, (interleaved_len+4));

                request->is_new_response = 1;
                request->is_interleaved_message = 0;
                continue;
            }
            else
            {
                request->is_new_response = 0;
                request->is_interleaved_message = 0;

                /* 认为是常规的rtsp文本消息开始，尝试解析之 */
                ret = rtsp_response_msg_decode(request->response_msg, data, size, &parsed_bytes);
                if (ret == 0)
                {
                    request_filt_response(request, connection, request->current_request, request->response_msg);
                    
                    request->is_in_handler = 1;
                    request->request_handler(request, connection, request->current_request, request->response_msg, request->userdata);
                    request->is_in_handler = 0;

                    rtsp_response_msg_unref(request->response_msg);
                    request->response_msg = NULL;
                    buffer_retrieve(buffer, parsed_bytes);

                    if (0 == request->is_alive)
                    {
                        request_delete(request);

                        break;
                    }

                    request->response_msg = rtsp_response_msg_new();
                    request->is_new_response = 1;
                    continue;
                }
                else if (ret > 0)
                {
                    /* 请求行解析没有出错，但数据不完整，无法继续，返回继续收数据 */
                    return;
                }
                else
                {
                    /* 解析出错，为非法消息 */
                    request->is_in_handler = 1;
                    request->request_handler(request, NULL, RTSP_METHOD_NONE, NULL, request->userdata);
                    request->is_in_handler = 0;

                    if (0 == request->is_alive)
                    {
                        request_delete(request);
                    }
                    
                    return;
                }
            }
        }
    }
    else
    {
        while (1)
        {
            data = (char*)buffer_peek(buffer);
            size = buffer_readablebytes(buffer);

            /* 实际上若是前一次不完整的interleaved消息，则起始字节一定是 0x24 */

            ch = data[0];
            if (ch == 0x24)
            {
                request->is_new_response = 0;
                request->is_interleaved_message = 1;
                
                ch = data[1];
                interleaved_len = ntohs(*(uint16_t*)&data[2]);
                if (size < (unsigned)(interleaved_len+4))
                {
                    return;
                }

                request->interleaved_sink(request, (unsigned char)ch, &data[4], interleaved_len, request->userdata);
                buffer_retrieve(buffer, (interleaved_len+4));

                request->is_new_response = 1;

                continue;
            }
            else
            {
                request->is_new_response = 0;
                request->is_interleaved_message = 0;

                request->response_msg = rtsp_response_msg_new();

                /* 认为是常规的rtsp文本消息开始，尝试解析之 */
                ret = rtsp_response_msg_decode(request->response_msg, data, size, &parsed_bytes);
                if (ret == 0)
                {
                    request_filt_response(request, connection, request->current_request, request->response_msg);
                    
                    request->is_in_handler = 1;
                    request->request_handler(request, connection, request->current_request, request->response_msg, request->userdata);
                    request->is_in_handler = 0;

                    rtsp_response_msg_unref(request->response_msg);
                    request->response_msg = NULL;
                    buffer_retrieve(buffer, parsed_bytes);

                    if (0 == request->is_alive)
                    {
                        request_delete(request);

                        break;
                    }

                    request->response_msg = rtsp_response_msg_new();
                    request->is_new_response = 1;
                    continue;
                }
                else if (ret > 0)
                {
                    /* 请求行解析没有出错，但数据不完整，无法继续，返回继续收数据 */
                    return;
                }
                else
                {
                    /* 解析出错，为非法消息 */
                    request->is_in_handler = 1;
                    request->request_handler(request, NULL, RTSP_METHOD_NONE, NULL, request->userdata);
                    request->is_in_handler = 0;
                    
                    if (0 == request->is_alive)
                    {
                        request_delete(request);
                    }
                    
                    return;
                }
            }
        }
    }

    return;
}

static 
void request_onclose(tcp_connection_t* connection, void* userdata)
{
    rtsp_request_t *request = (rtsp_request_t*)userdata;
    
    request->is_in_handler = 1;
    request->request_handler(request, connection, RTSP_METHOD_NONE, NULL, request->userdata);
    request->is_in_handler = 0;

    if (0 == request->is_alive)
    {
        request_delete(request);
    }

    return;
}

static 
void request_onconnected(tcp_connection_t* connection, void *userdata)
{
    rtsp_request_t *request = (rtsp_request_t*)userdata;

    request->is_in_handler = 1;
    request->request_handler(request, connection, RTSP_METHOD_NONE, NULL, request->userdata);
    request->is_in_handler = 0;

    if (0 == request->is_alive)
    {
        request_delete(request);
    }

    return;
}

rtsp_request_t* rtsp_request_new
(
    loop_t* loop, 
    const char* url, 
    rtsp_request_handler_f request_handler, 
    rtsp_request_interleaved_packet_f interleaved_sink, 
    void* userdata
)
{
    rtsp_request_t *request;
    tcp_client_t *client;
    url_t *u;
    unsigned len;

    if (NULL == url || NULL == loop || NULL == request_handler || NULL == interleaved_sink)
    {
        log_error("rtsp_request_new: bad url(%p) or bad loop(%p) or bad request_handler(%p) or bad interleaved_sink(%p)", 
            url, loop, request_handler, interleaved_sink);
        return NULL;
    }

    u = url_parse(url, strlen(url));
    if (NULL == u)
    {
        log_error("rtsp_request_new: url_parse() failed");
        return NULL;
    }
    if (0 == u->port)
    {
        u->port = 554;
    }    

    request = (rtsp_request_t*)malloc(sizeof(rtsp_request_t));
    memset(request, 0, sizeof(*request));
    request->client = NULL;
    request->loop = loop;
    request->url = strdup(url);
    request->u = u;
    len = snprintf(request->pure_url, sizeof(request->pure_url)-1, "%s//%s:%u", u->schema, u->host, u->port);
    if (NULL != u->path)
    {
        snprintf(request->pure_url+len, sizeof(request->pure_url)-1-len, "%s", u->path);
    }

    request->request_handler = request_handler;
    request->interleaved_sink = interleaved_sink;
    request->userdata = userdata;
    
    request->response_msg = rtsp_response_msg_new();

    request->cseq = 0;
    request->current_request = RTSP_METHOD_NONE;
    request->got_sessionid = 0;
    request->sessionid = NULL;
    request->timeout = 0;
    request->need_auth = 0;
    request->is_in_handler = 0;
    request->is_alive = 1;

    client = tcp_client_new(loop, u->host, u->port, request_onconnected, request_ondata, request_onclose, request);
    if (NULL == client)
    {
        free(request);
        url_release(u);
        log_error("rtsp_request_new: tcp_client_new() failed");        
        return NULL;
    }
    request->client = client;

    return request;
}

void rtsp_request_destroy(rtsp_request_t* request)
{
    if (NULL == request)
    {
        return;
    }

    if (request->is_in_handler)
    {
        request->is_alive = 0;
    }
    else
    {
        request_delete(request);
    }

    return;
}

int rtsp_request_launch(rtsp_request_t* request)
{
    int result;
    if (NULL == request || NULL == request->client)
    {
        log_error("rtsp_request_launch: bad request(%p) or bad request->client(%p)", request, request->client);
        return -1;
    }

    result = tcp_client_connect(request->client);
    if (result < 0)
    {
        log_error("rtsp_request_launch: tcp_client_connect() failed");
    }

    return result;
}

unsigned rtsp_request_server_method(rtsp_request_t* request)
{
    return NULL == request ? 0 : request->server_methods;
}

int rtsp_request_timeout(rtsp_request_t* request)
{
    return NULL == request ? 0 : request->timeout;
}

int rtsp_request_options(rtsp_request_t* request)
{
    char buffer[1024];
    int len;
    tcp_connection_t* connection;

    if (NULL == request)
    {
        return -1;
    }

    connection = tcp_client_getconnection(request->client);
    if (NULL == connection)
    {
        log_error("rtsp_request_options: bad connection");
        return -1;
    }

    request->cseq++;
    request->current_request = RTSP_METHOD_OPTIONS;

    memset(buffer, 0, sizeof(buffer));
    /* 为保证用户名和密码不在url中泄露，严格的的应该使用pure_url作为请求url */
    /* len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, "OPTIONS", request->pure_url, NULL, NULL, 0); */
    len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, RTSP_METHOD_OPTIONS, request->url, NULL, NULL, 0);
    tcp_connection_send(connection, buffer, len);

    return 0;
}

int rtsp_request_describe(rtsp_request_t* request, rtsp_head_t* head)
{
    char buffer[1024];
    int len;
    tcp_connection_t* connection;

    if (NULL == request)
    {
        return -1;
    }

    connection = tcp_client_getconnection(request->client);
    if (NULL == connection)
    {
        log_error("rtsp_request_describe: bad connection");
        return -1;
    }

    request->cseq++;
    request->current_request = RTSP_METHOD_DESCRIBE;

    memset(buffer, 0, sizeof(buffer));
    /* 为保证用户名和密码不在url中泄露，严格的应该使用pure_url作为请求url */
    /* len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, "DESCRIBE", request->pure_url, head, NULL, 0); */
    len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, RTSP_METHOD_DESCRIBE, request->url, head, NULL, 0);
    tcp_connection_send(connection, buffer, len);

    return 0;
}

int rtsp_request_setup(rtsp_request_t* request, rtsp_head_t* head, const char *url)
{
    char buffer[1024];
    int len;
    tcp_connection_t* connection;
    const char *u;
    rtsp_head_t session_head;
    rtsp_head_t* headptr;    

    if (NULL == request)
    {
        return -1;
    }

    connection = tcp_client_getconnection(request->client);
    if (NULL == connection)
    {
        log_error("rtsp_request_setup: bad connection");
        return -1;
    }

    request->cseq++;
    request->current_request = RTSP_METHOD_SETUP;
    
    if (request->got_sessionid)
    {
        session_head.key = RTSP_HEAD_SESSION;
        session_head.value = request->sessionid;
        session_head.next = head;

        headptr = &session_head;
    }
    else
    {
        headptr = head;
    }

    u = url;
    if (NULL == u)
    {
        u = request->pure_url;
        /* u = request->url; */
    }

    memset(buffer, 0, sizeof(buffer));
    len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, RTSP_METHOD_SETUP, u, headptr, NULL, 0);
    tcp_connection_send(connection, buffer, len);

    return 0;
}

int rtsp_request_play(rtsp_request_t* request, rtsp_head_t* head)
{
    char buffer[1024];
    int len;
    tcp_connection_t* connection;
    rtsp_head_t session_head;
    rtsp_head_t* headptr;    

    if (NULL == request)
    {
        return -1;
    }

    connection = tcp_client_getconnection(request->client);
    if (NULL == connection)
    {
        log_error("rtsp_request_play: bad connection");
        return -1;
    }

    request->cseq++;
    request->current_request = RTSP_METHOD_PLAY;

    if (request->got_sessionid)
    {
        session_head.key = RTSP_HEAD_SESSION;
        session_head.value = request->sessionid;
        session_head.next = head;

        headptr = &session_head;
    }
    else
    {
        headptr = head;
    }

    memset(buffer, 0, sizeof(buffer));
    /* len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, "PLAY", request->pure_url, headptr, NULL, 0); */
    len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, RTSP_METHOD_PLAY, request->pure_url, headptr, NULL, 0);
    tcp_connection_send(connection, buffer, len);

    return 0;
}

int rtsp_request_teardown(rtsp_request_t* request, rtsp_head_t* head)
{
    char buffer[1024];
    int len;
    tcp_connection_t* connection;
    rtsp_head_t session_head;
    rtsp_head_t* headptr;

    if (NULL == request)
    {
        return -1;
    }
    
    connection = tcp_client_getconnection(request->client);
    if (NULL == connection)
    {
        log_warn("rtsp_request_teardown: the connection for %s has not been made", request->url);
        return -1;
    }

    request->cseq++;
    request->current_request = RTSP_METHOD_TEARDOWN;

    if (request->got_sessionid)
    {
        session_head.key = RTSP_HEAD_SESSION;
        session_head.value = request->sessionid;
        session_head.next = head;

        headptr = &session_head;
    }
    else
    {
        headptr = head;
    }

    memset(buffer, 0, sizeof(buffer));
    len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, RTSP_METHOD_TEARDOWN, request->pure_url, headptr, NULL, 0);
    /* len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, "TEARDOWN", request->url, headptr, NULL, 0); */
    tcp_connection_send(connection, buffer, len);

    return 0;
}

int rtsp_request_get_parameter(rtsp_request_t* request, rtsp_head_t* head, const char* body, unsigned body_len)
{
    char buffer[1024];
    int len;
    tcp_connection_t* connection;
    rtsp_head_t session_head;
    rtsp_head_t* headptr;

    if (NULL == request)
    {
        return -1;
    }

    connection = tcp_client_getconnection(request->client);
    if (NULL == connection)
    {
        log_error("rtsp_request_get_parameter: bad connection");
        return -1;
    }

    request->cseq++;
    request->current_request = RTSP_METHOD_GET_PARAMETER;
    
    if (request->got_sessionid)
    {
        session_head.key = RTSP_HEAD_SESSION;
        session_head.value = request->sessionid;
        session_head.next = head;

        headptr = &session_head;
    }
    else
    {
        headptr = head;
    }

    memset(buffer, 0, sizeof(buffer));
    /* 为保证用户名和密码不在url中泄露，严格的的应该使用pure_url作为请求url */
    /* len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, "OPTIONS", request->pure_url, NULL, NULL, 0); */
    len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, RTSP_METHOD_GET_PARAMETER, request->pure_url, headptr, body, body_len);
    tcp_connection_send(connection, buffer, len);

    return 0;
}

int rtsp_request_set_parameter(rtsp_request_t* request, rtsp_head_t* head, const char* body, unsigned body_len)
{
    char buffer[1024];
    int len;
    tcp_connection_t* connection;
    rtsp_head_t session_head;
    rtsp_head_t* headptr;

    if (NULL == request || NULL == body || 0 == body_len)
    {
        return -1;
    }

    connection = tcp_client_getconnection(request->client);
    if (NULL == connection)
    {
        log_error("rtsp_request_get_parameter: bad connection");
        return -1;
    }

    request->cseq++;
    request->current_request = RTSP_METHOD_SET_PARAMETER;
    
    if (request->got_sessionid)
    {
        session_head.key = RTSP_HEAD_SESSION;
        session_head.value = request->sessionid;
        session_head.next = head;

        headptr = &session_head;
    }
    else
    {
        headptr = head;
    }

    memset(buffer, 0, sizeof(buffer));
    /* 为保证用户名和密码不在url中泄露，严格的的应该使用pure_url作为请求url */
    len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, RTSP_METHOD_SET_PARAMETER, request->pure_url, headptr, body, body_len);
    /* len = rtsp_msg_buid_request(buffer, sizeof(buffer), request->cseq, RTSP_METHOD_SET_PARAMETER, request->url, headptr, body, body_len); */
    tcp_connection_send(connection, buffer, len);

    return 0;
}

