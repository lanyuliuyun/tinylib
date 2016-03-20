
#ifdef WINNT
    #include "tinylib/windows/net/tcp_connection.h"
    #include "tinylib/windows/net/buffer.h"
    
    #include <winsock2.h>
#elif defined(__linux__)
    #include "tinylib/linux/net/tcp_connection.h"
    #include "tinylib/linux/net/buffer.h"
    
    #include <arpa/inet.h>
#endif

#include "tinylib/rtsp/rtsp_message_codec.h"
#include "tinylib/rtsp/rtsp_session.h"
#include "tinylib/util/log.h"

#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

struct rtsp_session
{
    tcp_connection_t* connection;

    rtsp_session_handler_f session_handler;
    rtsp_session_interleaved_packet_f interleaved_sink;
    void* userdata;

    rtsp_request_msg_t *request_msg;

    int is_new_request;                /* 标记一则新的rtsp客户端消息的开始，可能是新的rtsp请求消息，或interleaved消息
                                     * 在最开始和每处理完一则消息，需要将其置为1
                                     */

    int is_interleaved_message;        /* 标记是否是一则interleaved消息 */

    unsigned long context_data[4];
    void *extra_userdata;

    int is_in_handler;
    int is_alive;    
};

static inline
void session_delete(rtsp_session_t* session)
{
    tcp_connection_destroy(session->connection);
    rtsp_request_msg_unref(session->request_msg);
    free(session);

    return;
}

static 
void session_ondata(tcp_connection_t* connection, buffer_t* buffer, void* userdata)
{
    rtsp_session_t* session;

    unsigned parsed_bytes;
    char* data;
    unsigned size;
    uint8_t ch;

    uint16_t interleaved_len;
    int ret;

    session = (rtsp_session_t*)userdata;

    if (session->is_new_request)
    {
        while (1)
        {
            data = (char*)buffer_peek(buffer);
            size = buffer_readablebytes(buffer);

            ch = data[0];
            if (ch == 0x24)
            {
                session->is_new_request = 0;
                session->is_interleaved_message = 1;

                ch = data[1];
                interleaved_len = ntohs(*(uint16_t*)&data[2]);
                if (size < (unsigned)(interleaved_len+4))
                {
                    return;
                }

                session->interleaved_sink(session, (unsigned char)ch, &data[4], interleaved_len, session->userdata);

                buffer_retrieve(buffer, (interleaved_len+4));
                session->is_new_request = 1;
                session->is_interleaved_message = 0;
                continue;
            }
            else
            {
                session->is_new_request = 0;
                session->is_interleaved_message = 0;

                /* 认为是常规的rtsp文本消息开始，尝试解析之 */
                ret = rtsp_request_msg_decode(session->request_msg, data, size, &parsed_bytes);
                if (ret == 0)
                {
                    session->is_in_handler = 1;
                    session->session_handler(session, connection, session->request_msg, session->userdata);
                    session->is_in_handler = 0;

                    rtsp_request_msg_unref(session->request_msg);
                    session->request_msg = NULL;
                    buffer_retrieve(buffer, parsed_bytes);

                    if (0 == session->is_alive)
                    {
                        session_delete(session);

                        break;
                    }

                    session->request_msg = rtsp_request_msg_new();
                    session->is_new_request = 1;
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
                    session->is_in_handler = 1;
                    session->session_handler(session, NULL, NULL, session->userdata);
                    session->is_in_handler = 0;
                    
                    if (0 == session->is_alive)
                    {
                        session_delete(session);
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
                session->is_new_request = 0;
                session->is_interleaved_message = 1;

                ch = data[1];
                interleaved_len = ntohs(*(uint16_t*)&data[2]);
                if (size < (unsigned)(interleaved_len+4))
                {
                    return;
                }

                session->interleaved_sink(session, (unsigned char)ch, &data[4], interleaved_len, session->userdata);

                buffer_retrieve(buffer, (interleaved_len+4));
                session->is_new_request = 1;
                session->is_interleaved_message = 0;
                continue;
            }
            else
            {
                session->is_new_request = 0;
                session->is_interleaved_message = 0;

                /* 认为是常规的rtsp文本消息开始，尝试解析之 */
                ret = rtsp_request_msg_decode(session->request_msg, data, size, &parsed_bytes);
                if (ret == 0)
                {
                    session->is_in_handler = 1;
                    session->session_handler(session, connection, session->request_msg, session->userdata);
                    session->is_in_handler = 0;

                    rtsp_request_msg_unref(session->request_msg);
                    session->request_msg = NULL;
                    buffer_retrieve(buffer, parsed_bytes);

                    if (0 == session->is_alive)
                    {
                        session_delete(session);

                        break;
                    }

                    session->request_msg = rtsp_request_msg_new();
                    session->is_new_request = 1;
                    continue;
                }
                else if (ret > 0)
                {
                    /* 请求行解析没有出错，但数据不够，无法继续，返回继续收数据 */
                    return;
                }
                else
                {
                    /* 解析出错，为非法消息 */
                    session->is_in_handler = 1;
                    session->session_handler(session, NULL, NULL, session->userdata);
                    session->is_in_handler = 0;

                    if (0 == session->is_alive)
                    {
                        session_delete(session);
                    }
                    
                    return;
                }
            }
        }
    }

    return;
}

static 
void session_onclose(tcp_connection_t* connection, void* userdata)
{
    rtsp_session_t* session = (rtsp_session_t*)userdata;

    session->is_in_handler = 1;
    session->session_handler(session, connection, NULL, session->userdata);
    session->is_in_handler = 0;

    if (0 == session->is_alive)
    {
        session_delete(session);
    }

    return;
}

rtsp_session_t* rtsp_session_start
(
    tcp_connection_t *connection, 
    rtsp_session_handler_f session_handler, 
    rtsp_session_interleaved_packet_f interleaved_sink, 
    void* userdata
)
{
    rtsp_session_t* session;

    if (NULL == connection || NULL == session_handler || NULL == interleaved_sink)
    {
        log_error("rtsp_session_start: bad connection(%p) or bad session_handler(%p) or bad interleaved_sink(%p)", 
            connection, session_handler, interleaved_sink);
        return NULL;
    }

    session = (rtsp_session_t*)malloc(sizeof(rtsp_session_t));

    memset(session, 0, sizeof(*session));
    tcp_connection_setcalback(connection, session_ondata, session_onclose, session);
    session->connection = connection;
    
    session->session_handler = session_handler;
    session->interleaved_sink = interleaved_sink;
    session->userdata = userdata;

    session->request_msg = rtsp_request_msg_new();

    session->is_new_request = 1;
    session->is_interleaved_message = 0;

    memset(session->context_data, 0, sizeof(session->context_data));
    session->extra_userdata = NULL;
    
    session->is_alive = 1;
    session->is_in_handler = 0;

    /* session创建完毕之后，向用户给一个通知，以方便其进行必要的初始化工作 */
    session_handler(session, connection, NULL, session->userdata);

    return session;
}

void rtsp_session_end(rtsp_session_t* session)
{
    if (NULL == session)
    {
        return;
    }

    if (session->is_in_handler)
    {
        session->is_alive = 0;
    }
    else
    {
        session_delete(session);
    }

    return;
}

void rtsp_session_set_extra_userdata(rtsp_session_t* session, void *userdata)
{
    if (NULL != session)
    {
        session->extra_userdata = userdata;
    }
    
    return;
}

void* rtsp_session_get_extra_userdata(rtsp_session_t* session)
{
    if (NULL == session)
    {
        return NULL;
    }

    return session->extra_userdata;
}
