
/** 实现一个简答的rtsp客户端，不支持pipeline方式的请求发送和应答处理 */

#ifndef RTSP_REQUEST_H
#define RTSP_REQUEST_H

struct rtsp_request;
typedef struct rtsp_request rtsp_request_t;

#if WINNT
    #include "tinylib/windows/net/tcp_connection.h"
#elif defined(__linux__)
    #include "tinylib/linux/net/tcp_connection.h"
#endif

#include "tinylib/rtsp/rtsp_message_codec.h"

#ifdef __cplusplus
extern "C" {
#endif

/** rtsp请求响应消息的处理回调
  *
  * @connnection: 对应本次会话的tcp连接
  * @method: 收到此响应消息之前，client端发送请求消息中的请求方法，
  *          如果是RTSP_METHOD_NONE表示与远端RTSP服务器的连接建立OK，此时response为NULL
  * @respons: 响应消息，不包含body部分
  */
typedef void (*rtsp_request_handler_f)
(
    rtsp_request_t *request, 
    tcp_connection_t* connection, 
    rtsp_method_e method, 
    const rtsp_response_msg_t* response, 
    void *userdata
);

typedef void (*rtsp_request_interleaved_packet_f)
(
    rtsp_request_t *request, 
    unsigned char channel,
    void* packet, unsigned short size,
    void *userdata
);

rtsp_request_t* rtsp_request_new
(
    loop_t* loop, 
    const char* url, 
    rtsp_request_handler_f handler, 
    rtsp_request_interleaved_packet_f interleaved_sink, 
    void* userdata
);

void rtsp_request_destroy(rtsp_request_t* request);

int rtsp_request_launch(rtsp_request_t* request);

/* 返回远端服务器支持的方法，仅当OPTIONS请求正确响应之后才有效，其他返回0 */
unsigned rtsp_request_server_method(rtsp_request_t* request);

/* 获取服务器返回的回话超时值 */
int rtsp_request_timeout(rtsp_request_t* request);

int rtsp_request_options(rtsp_request_t* request);

int rtsp_request_describe(rtsp_request_t* request, rtsp_head_t* head);

/* 额外的url参数指定具体为哪个SDP中的子媒体流进行SETUP，
  * 若为NULL，则将默认使用请求创建时指定的URL
  */
int rtsp_request_setup(rtsp_request_t* request, rtsp_head_t* head, const char *url);

int rtsp_request_play(rtsp_request_t* request, rtsp_head_t* head);

int rtsp_request_teardown(rtsp_request_t* request, rtsp_head_t* head);

int rtsp_request_get_parameter(rtsp_request_t* request, rtsp_head_t* head, const char* body, unsigned body_len);

int rtsp_request_set_parameter(rtsp_request_t* request, rtsp_head_t* head, const char* body, unsigned body_len);

#ifdef __cplusplus
}
#endif

#endif /*  !RTSP_REQUEST_H */

