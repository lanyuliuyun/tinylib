
/* 对rtsp消息进行解析，生成对应的request/response对象 
 * 仅做简单解析，对于head中value字段，请在各自使用时再做进一步解析
 */

#ifndef RTSP_MESSAGE_CODEC_H
#define RTSP_MESSAGE_CODEC_H

typedef enum rtsp_method{
    RTSP_METHOD_NONE = 0,
    RTSP_METHOD_OPTIONS = 1,
    RTSP_METHOD_DESCRIBE = 1<<1,
    RTSP_METHOD_ANNOUNCE = 1<<2,
    RTSP_METHOD_SETUP = 1<<3,
    RTSP_METHOD_PLAY = 1<<4,
    RTSP_METHOD_PAUSE = 1<<5,
    RTSP_METHOD_TEARDOWN = 1<<6,
    RTSP_METHOD_GET_PARAMETER = 1<<7,
    RTSP_METHOD_SET_PARAMETER = 1<<8,
    RTSP_METHOD_REDIRECT = 1<<9,
    RTSP_METHOD_RECORD = 1<<10,
}rtsp_method_e;

typedef enum rtsp_head_key{
    RTSP_HEAD_ACCEPT,
    RTSP_HEAD_ACCEPT_ENCODING,
    RTSP_HEAD_ACCEPT_LANGUAGE,
    RTSP_HEAD_ALLOW,
    RTSP_HEAD_AUTHORIZATION,
    RTSP_HEAD_BANDWIDTH,
    RTSP_HEAD_BLOCKSIZE,
    RTSP_HEAD_CACHE_CONTROL,
    RTSP_HEAD_CONFERENCE,
    RTSP_HEAD_CONNECTION,
    RTSP_HEAD_CONTENT_BASE,
    RTSP_HEAD_CONTENT_ENCODING,
    RTSP_HEAD_CONTENT_LANGUAGE,
    RTSP_HEAD_CONTENT_LENGTH,
    RTSP_HEAD_CONTENT_LOCATION,
    RTSP_HEAD_CONTENT_TYPE,
    RTSP_HEAD_CSEQ,
    RTSP_HEAD_DATE,
    RTSP_HEAD_EXPIRES,
    RTSP_HEAD_FROM,
    RTSP_HEAD_HOST,
    RTSP_HEAD_IF_MATCH,
    RTSP_HEAD_IF_MODIFIED_SINCE,
    RTSP_HEAD_LAST_MODIFIED,
    RTSP_HEAD_LOCATION,
    RTSP_HEAD_PROXY_AUTHENTICATE,
    RTSP_HEAD_PROXY_REQUIRE,
    RTSP_HEAD_PUBLIC,
    RTSP_HEAD_RANGE,
    RTSP_HEAD_REFERER,
    RTSP_HEAD_RETRY_AFTER,
    RTSP_HEAD_REQUIRE,
    RTSP_HEAD_RTP_INFO,
    RTSP_HEAD_SCALE,
    RTSP_HEAD_SPEED,
    RTSP_HEAD_SERVER,
    RTSP_HEAD_SESSION,
    RTSP_HEAD_TIMESTAMP,
    RTSP_HEAD_TRANSPORT,
    RTSP_HEAD_UNSUPPORTED,
    RTSP_HEAD_USER_AGENT,
    RTSP_HEAD_VARY,
    RTSP_HEAD_VIA,
    RTSP_HEAD_WWW_AUTHENTICA
}rtsp_head_key_e;

typedef struct rtsp_head
{
    rtsp_head_key_e key;
    char* value;
    struct rtsp_head* next;
}rtsp_head_t;

typedef struct rtsp_request_msg
{
    rtsp_method_e method;       /* rtsp_method_e */
    char* url;
    int version;                /* 0x0100表示RTSP/1.0 */
    int cseq;                   /* 单独将消息的的cseq头拎出来，方便使用 */
    rtsp_head_t* head;            /* 除CSeq之外的head */
    char *body;
    int body_len;
}rtsp_request_msg_t;

typedef struct rtsp_response_msg
{
    int version;                /* 0x0100表示RTSP/1.0 */
    int code;                   /* 响应结果中的status code */
    int cseq;                   /* 对应于request消息中的cseq头，为方便做单独标记 */
    rtsp_head_t* head;          /* 除CSeq之外的head */
    char* body;                 /* 消息体内容 ，一般对应于DESCRIBE响应中的SDP信息 */
    int body_len;                /*消息体的长度，body存在时有效*/
}rtsp_response_msg_t;

typedef struct rtsp_interleaved_head{
    unsigned char magic;
    unsigned char channel;
    unsigned short len;
}rtsp_interleaved_head_t;

typedef struct rtsp_transport_head
{
    const char *trans;
    const char* cast;
    const char* destination;
    const char* source;
    unsigned short client_rtp_port;
    unsigned short client_rtcp_port;
    unsigned short server_rtp_port;
    unsigned short server_rtcp_port;
    const char* ssrc;
    unsigned interleaved;
    unsigned char rtp_channel;
    unsigned char rtcp_channel;
}rtsp_transport_head_t;

typedef struct rtsp_authenticate_head
{
    const char *type;
    const char *realm;
    const char *nonce;
    const char *stale;
}rtsp_authenticate_head_t;

#ifdef __cplusplus
extern "C" {
#endif

/* 创建一个空的rtsp请求消息对象 */
rtsp_request_msg_t* rtsp_request_msg_new(void);

/* 销毁一个给定的请求消息对象 */
void rtsp_request_msg_destroy(rtsp_request_msg_t* request_msg);

/* 类似于expat流式方式输入进行解析
 * 返回0 表示成功解析到一个完整的rtsp请求消息
 * 返回1 表示解析过程没有错误，但因数据不够完整的rtsp请求消息尚未解析完成
 * 返回-1 表示解析过程出错，多是遇到非法的消息
 * 
 * parsed_bytes 表示成功解析到一个rtsp请求消息时（即返回值为0时），消耗了多少字节；其他情况，该输出参数无意义
 */
int rtsp_request_msg_decode(rtsp_request_msg_t* request_msg, const char *data, unsigned size, unsigned *parsed_bytes);

/* 对请求消息进行原子性的引用计数操作，主要在跨线程的环境里使用 */
/** 增加一次引用计数 */
void rtsp_request_msg_ref(rtsp_request_msg_t* request_msg);
/** 减少一次引用计数，并返回操作后的计数值，当计数值为0时，该对象将被销毁 */
int rtsp_request_msg_unref(rtsp_request_msg_t* request_msg);

rtsp_response_msg_t* rtsp_response_msg_new(void);

/* 类似于expat流式方式输入进行解析
 * 返回0 表示成功解析到一个完整的rtsp请求消息
 * 返回1 表示解析过程没有错误，但因数据不够完整的rtsp请求消息尚未解析完成
 * 返回-1 表示解析过程出错，多是遇到非法的消息
 * 
 * parsed_bytes 表示成功解析到一个rtsp请求消息时（即返回值为0时），消耗了多少字节；其他情况，该输出参数无意义
 */
int rtsp_response_msg_decode(rtsp_response_msg_t* response_msg, const char *data, unsigned size, unsigned *parsed_bytes);

void rtsp_response_msg_destroy(rtsp_response_msg_t* response_msg);

/* 对响应消息进行原子性的引用计数操作，主要在跨线程的环境里使用 */
/** 增加一次引用计数 */
void rtsp_response_msg_ref(rtsp_response_msg_t* response_msg);
/** 减少一次引用计数，并返回操作后的计数值，当计数值为0时，该对象将被销毁 */
int rtsp_response_msg_unref(rtsp_response_msg_t* response_msg);

/** 构建相应消息，其中time-header和cseq-header 内部会自行填充，其余的header由head指定 */
int rtsp_msg_build_response
(
    char *response_msg, unsigned len, int cseq, int code,
    rtsp_head_t* head, const char* body, unsigned body_len
);

int rtsp_msg_buid_request
(
    char* request_msg, unsigned len, int cseq, rtsp_method_e method, const char* url, 
    rtsp_head_t* head, const char* body, unsigned body_len
);

rtsp_transport_head_t* rtsp_transport_head_decode(const char *transport);
void rtsp_transport_head_destroy(rtsp_transport_head_t* head);

rtsp_authenticate_head_t* rtsp_authenticate_head_decode(const char *auth);
void rtsp_authenticate_head_destroy(rtsp_authenticate_head_t* head);

/* 根据给定的user, password, method, url, realm, nonce计算response, 并组织authorization信息 */
unsigned rtsp_authorization_head
(
    char *auth, unsigned len, const char *user, const char *password, 
    const char *method, const char *url, const char *realm, const char *nonce
);

#ifdef __cplusplus
}
#endif

#endif /* !RTSP_MESSAGE_CODEC_H */

