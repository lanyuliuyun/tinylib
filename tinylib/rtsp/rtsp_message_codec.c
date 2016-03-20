
/** 主要是基于builder模式对rtsp请求消息和响应消息进行解析 */

#include "tinylib/rtsp/rtsp_message_codec.h"

#include "tinylib/util/log.h"
#include "tinylib/util/atomic.h"
#include "tinylib/util/md5.h"     /* for MD5() */

#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

enum rtsp_msg_parse_state{
    sw_start = 0,
    sw_method,
    sw_spaces_before_uri,
    sw_uri,
    sw_spaces_after_uri,
    sw_version,
    sw_rl_crlf,
    sw_space_after_version,
    sw_status_code,
    sw_space_after_status_code,
    sw_status_text,
    sw_head_key,
    sw_head_colon,
    sw_head_value,
    sw_head_crlf,
    sw_head_end,
    sw_body,
};

struct request_msg_private
{
    atomic_t ref_count;
    int data_ptr_offset;

    enum rtsp_msg_parse_state state;

    const char *method_start;
    const char *method_end;
    
    const char *uri_start;
    const char *uri_end;
    
    const char *version_start;
    const char *version_end;
    
    const char *head_key_start;
    const char *head_key_end;
    const char *head_value_start;
    const char *head_value_end;

    const char *body_start;
};

struct response_msg_private
{
    atomic_t ref_count;
    int data_ptr_offset;

    enum rtsp_msg_parse_state state;
    
    const char *version_start;
    const char *version_end;
    
    const char *status_code_start;
    const char *status_code_end;

    const char *status_text_start;
    const char *status_text_end;
    
    const char *head_key_start;
    const char *head_key_end;
    const char *head_value_start;
    const char *head_value_end;

    const char *body_start;
};

static inline
const char *get_status_text(int code)
{
    const char* status = NULL;
    
    static const char* success_text[] = {
        "OK",    /* 200 */
        NULL
    };

    static const char* redirect_text[] = {
        "Multiple Choices",        /* 300 */
        "Moved Permanently",     /* 301 */
        "Moved Temporarily",    /* 302 */
        "See Other",            /* 303 */
        "Not Modified",            /* 304 */
        "Use Proxy",            /* 305 */
        NULL
    };
    
    static const char* client_error_text[] = {
        "Bad Request",                        /* 400 */
        "Unauthorized",                        /* 401 */
        "Payment Required",                    /* 402 */
        "Forbidden",                        /* 403 */
        "Not Found",                        /* 404 */
        "Method Not Allowed",                /* 405 */
        "Not Acceptable",                    /* 406 */
        "Proxy Authentication Required",    /* 407 */
        "Request Timeout",                    /* 408 */
        "Conflict",                            /* 409 */
        "Gone",                                /* 410 */
        "Length Required",                    /* 411 */
        NULL
    };

    static const char* server_error_text[] = {
        "Internal Server Error",            /* 500 */
        "Not Implemented",                    /* 501 */
        "Bad Gateway",                        /* 502 */
        "Service Unavailable",                /* 503 */
        "Gateway Timeout",                    /* 504 */
        NULL
    };
    
    if (code >= 200 && code < 300)
    {
        if (code == 200)
        {
            status = success_text[code - 200];
        }
    }
    else if (code < 306)
    {
        status = redirect_text[code - 300];
    }
    else if (code >= 400 && code < 412)
    {
        status = client_error_text[code - 400];
    }
    else if (code >= 500 && code < 505)
    {
        status = server_error_text[code - 300];
    }
    
    if (NULL == status)
    {
        status = "OK";
    }
    
    return status;
}

static const char* head_key_text[] = {
    "Accept",
    "Accept-Encoding",
    "Accept-Language",
    "Allow",
    "Authorization",
    "Bandwidth",
    "Blocksize",
    "Cache-Control",
    "Conference",
    "Connection",
    "Content-Base",
    "Content-Encoding",
    "Content-Language",
    "Content-Length",
    "Content-Location",
    "Content-Type",
    "CSeq",
    "Date",
    "Expires",
    "From",
    "Host",
    "If-Match",
    "If-Modified-Since",
    "Last-Modified",
    "Location",
    "Proxy-Authenticate",
    "Proxy-Require",
    "Public",
    "Range",
    "Referer",
    "Retry-After",
    "Require",
    "RTP-Info",
    "Scale",
    "Speed",
    "Server",
    "Session",
    "Timestamp",
    "Transport",
    "Unsupported",
    "User-Agent",
    "Vary",
    "Via",
    "WWW-Authentica",
    NULL
};

static const char* method_text[] = {
    "OPTIONS",
    "DESCRIBE",
    "ANNOUNCE",
    "SETUP",
    "PLAY",
    "PAUSE",
    "TEARDOWN",
    "GET_PARAMETER",
    "SET_PARAMETER",
    "REDIRECT",
    "RECORD",
    NULL
};

/* 以下字符串比较宏摘自 nginx ，并根据 rtsp 消息自身的特征，做了扩展
 * 并且 rtsp 消息解析过程也是参考了 nginx 中的解析过程
 * nginx 是个NB的软件产品！
 */
#define ngx_str4cmp(m, c0, c1, c2, c3)                                        \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)

#define ngx_str5cmp(m, c0, c1, c2, c3, c4)                                    \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && m[4] == c4

#define ngx_str6cmp(m, c0, c1, c2, c3, c4, c5)                                \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && (((uint32_t*) m)[1] & 0xffff) == ((c5 << 8) | c4)

#define ngx_str7cmp(m, c0, c1, c2, c3, c4, c5, c6)                            \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && (((uint32_t*) m)[1] & 0xffff) == ((c5 << 8) | c4)                  \
        && m[6] == c6

#define ngx_str8cmp(m, c0, c1, c2, c3, c4, c5, c6, c7)                        \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && ((uint32_t *) m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)

#define ngx_str9cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8)                    \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && ((uint32_t *) m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)  \
        && m[8] == c8

#define ngx_str10cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9)               \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && ((uint32_t *) m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)  \
        && (((uint32_t *) m)[2] & 0xffff) == ((c9 << 8) | c8)
        
#define ngx_str12cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11)     \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && ((uint32_t *) m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)  \
        && ((uint32_t *) m)[2] == ((c11 << 24) | (c10 << 16) | (c9 << 8) | c8)

#define ngx_str13cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12) \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)              \
        && ((uint32_t *) m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)   \
        && ((uint32_t *) m)[2] == ((c11 << 24) | (c10 << 16) | (c9 << 8) | c8) \
        && m[12] == c12
        
#define ngx_str14cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13) \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && ((uint32_t *)m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)  \
        && ((uint32_t *)m)[2] == ((c11 << 24) | (c10 << 16) | (c9 << 8) | c8) \
        && (((uint32_t *)m)[3] & 0xffff) == ((c13 << 8) | c12)

#define ngx_str16cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15)     \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && ((uint32_t *)m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)  \
        && ((uint32_t *)m)[2] == ((c11 << 24) | (c10 << 16) | (c9 << 8) | c8) \
        && ((uint32_t *)m)[3] == ((c15 << 24) | (c14 << 16) | (c13 << 8) | c12)

#define ngx_str17cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16)     \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && ((uint32_t *)m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)  \
        && ((uint32_t *)m)[2] == ((c11 << 24) | (c10 << 16) | (c9 << 8) | c8) \
        && ((uint32_t *)m)[3] == ((c15 << 24) | (c14 << 16) | (c13 << 8) | c12) \
        && m[16] == c16
        
#define ngx_str18cmp(m, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, c10, c11, c12, c13, c14, c15, c16, c17)     \
    *(uint32_t *) m == ((c3 << 24) | (c2 << 16) | (c1 << 8) | c0)             \
        && ((uint32_t *)m)[1] == ((c7 << 24) | (c6 << 16) | (c5 << 8) | c4)  \
        && ((uint32_t *)m)[2] == ((c11 << 24) | (c10 << 16) | (c9 << 8) | c8) \
        && ((uint32_t *)m)[3] == ((c15 << 24) | (c14 << 16) | (c13 << 8) | c12) \
        && (((uint32_t *)m)[4] & 0xffff) == ((c17 << 8) | c16)

static inline
void save_rtsp_head(rtsp_head_t* head, rtsp_head_t** head_list)
{
    if (NULL == *head_list)
    {
        *head_list = head;
    }
    else
    {
        head->next = *head_list;
        *head_list = head;
    }

    return;
}

/* 处理每对rtsp head，只支持rfc2326标准规定的head
 * 目前只处理常见的head，不常用的head，直接返回NULL
 */
static inline
rtsp_head_t* rtsp_msg_head_parse(const char *key_start, const char *key_end, const char *value_start, const char *value_end)
{
    rtsp_head_t* head = NULL;
    
    int key_len = key_end - key_start + 1;
    switch (key_len)
    {
        case 3:
        {
            if (ngx_str4cmp(key_start, 'V', 'i', 'a', ':'))
            {
            }
            break;
        }
        case 4:
        {
            if (ngx_str4cmp(key_start, 'C', 'S', 'e', 'q'))
            {
                head = (rtsp_head_t*)malloc(sizeof(rtsp_head_t) + (value_end - value_start + 2));
                head->key = RTSP_HEAD_CSEQ;
                head->value = (char*)&head[1];
                head->next = NULL;
                memcpy(head->value, value_start, (value_end - value_start + 1));
                head->value[value_end - value_start + 1] = 0;
            }
            else if (ngx_str4cmp(key_start, 'D', 'a', 't', 'e'))
            {
            }
            else if (ngx_str4cmp(key_start, 'F', 'r', 'o', 'm'))
            {
            }
            else if (ngx_str4cmp(key_start, 'H', 'o', 's', 't'))
            {
                /* This HTTP request header field is not needed for RTSP. 
                 * It should be silently ignored if sent. 
                 */
            }
            else if (ngx_str4cmp(key_start, 'V', 'a', 'r', 'y'))
            {
            }
            break;
        }
        case 5:
        {
            if (ngx_str5cmp(key_start, 'A', 'l', 'l', 'o', 'w'))
            {
                /* The Allow response header field lists the methods supported by the resource identified by the request-URI. */
            }
            else if (ngx_str5cmp(key_start, 'R', 'a', 'n', 'g', 'e'))
            {
                /* This request and response header field specifies a range of time. The range can be specified in a number of units. */
            }
            else if (ngx_str5cmp(key_start, 'S', 'c', 'a', 'l', 'e'))
            {
                /* A scale value of 1 indicates normal play or record at the normal
                 * forward viewing rate. If not 1, the value corresponds to the rate
                 * with respect to normal viewing rate. 
                 */
            }
            else if (ngx_str5cmp(key_start, 'S', 'p', 'e', 'e', 'd'))
            {
                /* This request header fields parameter requests the server to deliver
                 * data to the client at a particular speed, contingent on the server's
                 * ability and desire to serve the media stream at the given speed.
                 */
            }

            break;
        }
        case 6:
        {
            if (ngx_str6cmp(key_start, 'A', 'c', 'c', 'e', 'p', 't'))
            {
                /* The Accept request-header field can be used to specify certain
                 * presentation description content types which are acceptable for the response. 
                 */
            }
            else if (ngx_str6cmp(key_start, 'P', 'u', 'b', 'l', 'i', 'c'))
            {
                /* The Public response-header field lists the set of methods supported by the server. 
                 * The purpose of this field is strictly to inform the recipient of the capabilities of the server 
                 * regarding unusualmethods. 
                 */
                head = (rtsp_head_t*)malloc(sizeof(rtsp_head_t) + (value_end - value_start + 2));
                head->key = RTSP_HEAD_PUBLIC;
                head->value = (char*)&head[1];
                head->next = NULL;
                memcpy(head->value, value_start, (value_end - value_start + 1));
                head->value[value_end - value_start + 1] = 0;
            }
            else if (ngx_str6cmp(key_start, 'S', 'e', 'r', 'v', 'e', 'r'))
            {
                /* The Server response-header field contains information about 
                 * the software used by the origin server to handle the request. 
                 */
            }
            break;
        }
        case 7:
        {
            if (ngx_str8cmp(key_start, 'E', 'x', 'p', 'i', 'r', 'e', 's', ':'))
            {
                /* The Expires entity-header field gives the date/time 
                 * after which the response should be considered stale. 
                 */
            }
            else if (ngx_str8cmp(key_start, 'R', 'e', 'f', 'e', 'r', 'e', 'r', ':'))
            {
                /* The Referer[sic] request-header field allows the client to specify,
                 * for the server's benefit, the address (URI) of the resource from
                 * which the Request-URI was obtained (the "referrer", although the
                 * header field is misspelled.) 
                 */
            }
            else if (ngx_str8cmp(key_start, 'R', 'e', 'q', 'u', 'i', 'r', 'e', ':'))
            {
                /* The Require header is used by clients to query the server about options that it may or may not support. */
            }
            else if (ngx_str8cmp(key_start, 'S', 'e', 's', 's', 'i', 'o', 'n', ':'))
            {
                /* This request and response header field identifies an RTSP session started by the media server in a SETUP response 
                 * and concluded by TEARDOWN on the presentation URL. 
                 */
                head = (rtsp_head_t*)malloc(sizeof(rtsp_head_t) + (value_end - value_start + 2));
                head->key = RTSP_HEAD_SESSION;
                head->value = (char*)&head[1];
                head->next = NULL;
                memcpy(head->value, value_start, (value_end - value_start + 1));
                head->value[value_end - value_start + 1] = 0;
            }

            break;
        }
        case 8:
        {
            if (ngx_str8cmp(key_start, 'I', 'f', '-', 'M', 'a', 't', 'c', 'h'))
            {
            }
            else if (ngx_str8cmp(key_start, 'L', 'o', 'c', 'a', 't', 'i', 'o', 'n'))
            {
                /* The Location response-header field is used to redirect the recipient to a location 
                 * other than the Request-URI for completion of the request or identification of a new resource. 
                 */
            }
            else if (ngx_str8cmp(key_start, 'R', 'T', 'P', '-', 'I', 'n', 'f', 'o'))
            {
            }

            break;
        }
        case 9:
        {
            if (ngx_str9cmp(key_start, 'B', 'a', 'n', 'd', 'w', 'i', 'd', 't', 'h'))
            {
                /* The Bandwidth request header field describes the estimated bandwidth available to the client, 
                 * expressed as a positive integer and measured in bits per second. 
                 */
            }
            else if (ngx_str9cmp(key_start, 'B', 'l', 'o', 'c', 'k', 's', 'i', 'z', 'e'))
            {
            }
            else if (ngx_str9cmp(key_start, 'T', 'i', 'm', 'e', 's', 't', 'a', 'm', 'p'))
            {
            }
            else if (ngx_str9cmp(key_start, 'T', 'r', 'a', 'n', 's', 'p', 'o', 'r', 't'))
            {
                head = (rtsp_head_t*)malloc(sizeof(rtsp_head_t) + (value_end - value_start + 2));
                head->key = RTSP_HEAD_TRANSPORT;
                head->value = (char*)&head[1];
                head->next = NULL;
                memcpy(head->value, value_start, (value_end - value_start + 1));
                head->value[value_end - value_start + 1] = 0;
            }
            
            break;
        }
        case 10:
        {
            if (ngx_str10cmp(key_start, 'C', 'o', 'n', 'f', 'e', 'r', 'e', 'n', 'c', 'e'))
            {
            }
            else if (ngx_str10cmp(key_start, 'C', 'o', 'n', 'n', 'e', 'c', 't', 'i', 'o', 'n'))
            {
            }
            else if (ngx_str10cmp(key_start, 'U', 's', 'e', 'r', '-', 'A', 'g', 'e', 'n', 't'))
            {
            }
            
            break;
        }
        case 11:
        {
            if (ngx_str12cmp(key_start, 'R', 'e', 't', 'r', 'y', '-', 'A', 'f', 't', 'e', 'r', ':'))
            {
                /* The Retry-After response-header field can be used with a 503 (Service Unavailable) response to indicate 
                 * how long the service is expected to be unavailable to the requesting client. 
                 */
            }
            else if (ngx_str12cmp(key_start, 'U', 'n', 's', 'u', 'p', 'p', 'o', 'r', 't', 'e', 'd', ':'))
            {
                /* The Unsupported response header lists the features not supported by the server. */
            }
            break;
        }
        case 12:
        {
            if (ngx_str12cmp(key_start, 'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'B', 'a', 's', 'e'))
            {
                /* The Content-Base entity-header field may be used to specify the base URI for resolving relative URLs within the entity. */
            }
            else if (ngx_str12cmp(key_start, 'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'T', 'y', 'p', 'e'))
            {
                head = (rtsp_head_t*)malloc(sizeof(rtsp_head_t) + (value_end - value_start + 2));
                head->key = RTSP_HEAD_CONTENT_TYPE;
                head->value = (char*)&head[1];
                head->next = NULL;
                memcpy(head->value, value_start, (value_end - value_start + 1));
                head->value[value_end - value_start + 1] = 0;
            }
            break;
        }
        case 13:
        {
            if (ngx_str13cmp(key_start, 'A', 'u', 't', 'h', 'o', 'r', 'i', 'z', 'a', 't', 'i', 'o', 'n'))
            {
                /* A user agent that wishes to authenticate itself with a server--
                 * usually, but not necessarily, after receiving a 401 response--
                 * MAY do so by including an Authorization request-header field with the request. 
                 */
            }
            else if (ngx_str13cmp(key_start, 'C', 'a', 'c', 'h', 'e', '-', 'C', 'o', 'n', 't', 'r', 'o', 'l'))
            {
                
            }
            
            break;
        }
        case 14:
        {
            if (ngx_str14cmp(key_start, 'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'L', 'e', 'n', 'g', 't', 'h'))
            {
                head = (rtsp_head_t*)malloc(sizeof(rtsp_head_t) + (value_end - value_start + 2));
                head->key = RTSP_HEAD_CONTENT_LENGTH;
                head->value = (char*)&head[1];
                head->next = NULL;
                memcpy(head->value, value_start, (value_end - value_start + 1));
                head->value[value_end - value_start + 1] = 0;
            }
            else if (ngx_str14cmp(key_start, 'W', 'W', 'W', '-', 'A', 'u', 't', 'h', 'e', 'n', 't', 'i', 'c', 'a'))
            {
                
            }
            
            break;
        }
        case 15:
        {
            if (ngx_str16cmp(key_start, 'A', 'c', 'c', 'e', 'p', 't', '-', 'E', 'n', 'c', 'o', 'd', 'i', 'n', 'g', ':'))
            {
                
            }
            else if (ngx_str16cmp(key_start, 'A', 'c', 'c', 'e', 'p', 't', '-', 'L', 'a', 'n', 'g', 'u', 'a', 'g', 'e', ':'))
            {
                
            }
        }
        case 16:
        {
            if (ngx_str16cmp(key_start, 'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'E', 'n', 'c', 'o', 'd', 'i', 'n', 'g'))
            {
                
            }
            else if (ngx_str16cmp(key_start, 'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'L', 'a', 'n', 'g', 'u', 'a', 'g', 'e'))
            {
                
            }
            else if (ngx_str16cmp(key_start, 'C', 'o', 'n', 't', 'e', 'n', 't', '-', 'L', 'o', 'c', 'a', 't', 'i', 'o', 'n'))
            {
                
            }
        }
        case 17:
        {
            if (ngx_str17cmp(key_start, 'I', 'f', '-', 'M', 'o', 'd', 'i', 'f', 'i', 'e', 'd', '-', 'S', 'i', 'n', 'c', 'e'))
            {
            }
        }
        case 18:
        {
            if (ngx_str18cmp(key_start, 'P','r', 'o', 'x', 'y', '-', 'A', 'u', 't', 'h', 'e', 'n', 't', 'i', 'c', 'a', 't', 'e'))
            {
            }
        }
    }
    
    return head;
}

static inline
void rtsp_msg_head_destroy(rtsp_head_t* head)
{
    free(head);
    return;
}

rtsp_request_msg_t* rtsp_request_msg_new(void)
{
    rtsp_request_msg_t* request_msg;
    struct request_msg_private *priv;
    
    request_msg = (rtsp_request_msg_t*)malloc(sizeof(rtsp_request_msg_t) + sizeof(struct request_msg_private));
    memset(request_msg, 0, (sizeof(rtsp_request_msg_t) + sizeof(struct request_msg_private)));
    priv = (struct request_msg_private*)&request_msg[1];
    priv->ref_count = 1;
    priv->state = sw_start;
    
    return request_msg;
}

int rtsp_request_msg_decode(rtsp_request_msg_t* request_msg, const char *data, unsigned size, unsigned *parsed_bytes)
{
    struct request_msg_private *priv;
    
    const char *pos;
    const char *data_end;
    char ch;
    const char *method;
    
    enum rtsp_msg_parse_state state;

    rtsp_head_t* head;

    *parsed_bytes = 0;

    priv = (struct request_msg_private*)&request_msg[1];
    data_end = data + size;

    state = priv->state;
    pos = data + priv->data_ptr_offset;
    for (; pos < data_end; ++pos)
    {
        priv->data_ptr_offset = pos - data;

        ch = *pos;
        switch (state)
        {
            case sw_start:
            {
                if (ch == '\r' || ch == '\n')
                {
                    break;
                }

                if (ch >= 'A' && ch <= 'Z')
                {
                    priv->method_start = pos;
                    state = sw_method;
                    priv->state = state;
                    
                    break;
                }
                else
                {
                    log_error("rtsp_request_msg_decode: bad rtsp message start");
                    return -1;
                }

                break;
            }
            case sw_method:
            {
                if (ch == ' ')
                {
                    priv->method_end = pos - 1;
                    method = priv->method_start;

                    switch (pos - method)
                    {
                        case 4:
                        {
                            if (ngx_str4cmp(method, 'P', 'L', 'A', 'Y'))
                            {
                                request_msg->method = RTSP_METHOD_PLAY;
                            }
                            else
                            {
                                log_error("rtsp_request_msg_decode: bad 4 chars method");
                                return -1;
                            }

                            break;
                        }
                        case 5:
                        {
                            if (ngx_str5cmp(method, 'S', 'E', 'T', 'U', 'P'))
                            {
                                request_msg->method = RTSP_METHOD_SETUP;
                            }
                            else if (ngx_str5cmp(method, 'P', 'A', 'U', 'S', 'E'))
                            {
                                request_msg->method = RTSP_METHOD_PAUSE;
                            }
                            else
                            {
                                log_error("rtsp_request_msg_decode: bad 5 chars method");
                                return -1;
                            }
                            
                            break;
                        }
                        case 6:
                        {
                            if (ngx_str6cmp(method, 'R', 'E', 'C', 'O', 'R', 'D'))
                            {
                                request_msg->method = RTSP_METHOD_RECORD;
                            }
                            else
                            {
                                log_error("rtsp_request_msg_decode: bad 6 chars method");
                                return -1;
                            }
                            break;
                        }
                        case 7:
                        {
                            if (ngx_str8cmp(method, 'O', 'P', 'T', 'I', 'O', 'N', 'S', ' '))
                            {
                                request_msg->method = RTSP_METHOD_OPTIONS;
                            }
                            else
                            {
                                log_error("rtsp_request_msg_decode: bad 7 chars method");
                                return -1;
                            }
                            break;
                        }
                        case 8:
                        {
                            if (ngx_str8cmp(method, 'D', 'E', 'S', 'C', 'R', 'I', 'B', 'E'))
                            {
                                request_msg->method = RTSP_METHOD_DESCRIBE;
                            }
                            else if (ngx_str8cmp(method, 'T', 'E', 'A', 'R', 'D', 'O', 'W', 'N'))
                            {
                                request_msg->method = RTSP_METHOD_TEARDOWN;
                            }
                            else if (ngx_str8cmp(method, 'R', 'E', 'D', 'I', 'R', 'E', 'C', 'T'))
                            {
                                request_msg->method = RTSP_METHOD_REDIRECT;
                            }
                            else if (ngx_str8cmp(method, 'A', 'N', 'N', 'O', 'U', 'N', 'C', 'E'))
                            {
                                request_msg->method = RTSP_METHOD_ANNOUNCE;
                            }
                            else
                            {
                                log_error("rtsp_request_msg_decode: bad 8 chars method");
                                return -1;
                            }
                            break;
                        }
                        case 13:
                        {
                            if (ngx_str13cmp(method, 'G', 'E', 'T', '_', 'P', 'A', 'R', 'A', 'M', 'E', 'T', 'E', 'R'))
                            {
                                request_msg->method = RTSP_METHOD_GET_PARAMETER;
                            }
                            else if (ngx_str13cmp(method, 'S', 'E', 'T', '_', 'P', 'A', 'R', 'A', 'M', 'E', 'T', 'E', 'R'))
                            {
                                request_msg->method = RTSP_METHOD_SET_PARAMETER;
                            }
                            else
                            {
                                log_error("rtsp_request_msg_decode: bad 13 chars method");
                                return -1;
                            }
                            break;
                        }
                        default:
                        {
                            log_error("rtsp_request_msg_decode: bad method");
                            return -1;
                        }
                    }

                    state = sw_spaces_before_uri;
                    priv->state = state;

                    break;
                }

                if ((ch < 'A' || ch > 'Z') && ch != '_')
                {
                    log_error("rtsp_request_msg_decode: bad rtsp method char");
                    return -1;
                }

                break;
            }
            case sw_spaces_before_uri:
            {
                if (ch == ' ')
                {
                    break;
                }
                
                /* 可能请求的 uri 为相对path，不是完整的绝对uri，故此处不检查uri起始字符串 */
                priv->uri_start = pos;
                state = sw_uri;
                priv->state = state;

                break;
            }
            case sw_uri:
            {
                if (ch == ' ')
                {
                    priv->uri_end = pos - 1;

                    /* 校验uri是否合法 */
                    
                    if ((priv->uri_end - priv->uri_start) == 0)
                    {
                        /* uri为一个字符，但不是*，为非法的rtsp uri */
                        if (*priv->uri_start != '*')
                        {
                            log_error("rtsp_request_msg_decode: bad 1 char rtps uri");
                            return -1;
                        }
                    }
                    
                    /* 可能请求的 uri 为相对path，不是完整的绝对uri，故此处不检查uri起始字符串 */

                    request_msg->url = malloc(priv->uri_end - priv->uri_start + 2);
                    memcpy(request_msg->url, priv->uri_start, (priv->uri_end - priv->uri_start + 1));
                    request_msg->url[priv->uri_end - priv->uri_start + 1] = 0;

                    state = sw_spaces_after_uri;
                    priv->state = state;

                    break;
                }

                if (isascii(ch) == 0)
                {
                    /* uri中出现非ascii字符，为非法消息 */
                    log_error("rtsp_request_msg_decode: non-ascii char in uri");
                    return -1;
                }

                break;
            }
            case sw_spaces_after_uri:
            {
                if (ch == ' ')
                {
                    break;
                }

                priv->version_start = pos;

                state = sw_version;
                priv->state = state;

                break;
            }
            case sw_version:
            {
                if (ch == '\r')
                {
                    priv->version_end = pos-1;

                    if ((priv->version_end - priv->version_start) < 7)
                    {
                        log_error("rtsp_request_msg_decode: rtsp version string piece is too short");
                        return -1;
                    }

                    /* 检查RTSP版本信息 */
                    if (!ngx_str5cmp(priv->version_start, 'R', 'T', 'S', 'P', '/'))
                    {
                        log_error("rtsp_request_msg_decode: rtsp version is not started with 'RTSP/'");
                        return -1;
                    }
                    else
                    {
                        if (priv->version_start[5] < '0' || priv->version_start[5] > '9')
                        {
                            log_error("rtsp_request_msg_decode: bad rtsp main version number");
                            return -1;
                        }
                        else
                        {
                            request_msg->version |= (int)(priv->version_start[5] - '0') << 8;
                        }
                        
                        if (priv->version_start[6] != '.')
                        {
                            log_error("rtsp_request_msg_decode: bad rtsp version delimit");
                            return -1;
                        }

                        if (priv->version_start[7] < '0' || priv->version_start[7] > '9')
                        {
                            log_error("rtsp_request_msg_decode: bad rtsp minor version number");
                            return -1;
                        }
                        else
                        {
                            request_msg->version |= (int)(priv->version_start[7] - '0');
                        }
                    }

                    state = sw_rl_crlf;
                    priv->state = state;

                    break;
                }

                break;
            }
            case sw_rl_crlf:
            {
                if(ch != '\n')
                {
                    priv->head_key_start = pos;
                    state = sw_head_key;
                    priv->state = state;

                    break;
                }

                break;
            }
            case sw_head_key:
            {
                if (ch == ':')
                {
                    priv->head_key_end = pos - 1;
                    state = sw_head_colon;
                    priv->state = state;
                    
                    break;
                }

                break;
            }
            case sw_head_colon:
            {
                if (ch != ' ')
                {
                    priv->head_value_start = pos;
                    state = sw_head_value;
                    priv->state = state;
                    break;
                }

                break;
            }
            case sw_head_value:
            {
                if (ch == '\r')
                {
                    priv->head_value_end = pos - 1;

                    head = rtsp_msg_head_parse(priv->head_key_start, priv->head_key_end, priv->head_value_start, priv->head_value_end);
                    if (NULL != head)
                    {
                        if (head->key == RTSP_HEAD_CSEQ)
                        {
                            request_msg->cseq = atoi(head->value);
                            rtsp_msg_head_destroy(head);
                        }
                        else if (head->key == RTSP_HEAD_CONTENT_LENGTH)
                        {
                            request_msg->body_len = atoi(head->value);
                            rtsp_msg_head_destroy(head);
                        }
                        else
                        {
                            save_rtsp_head(head, &(request_msg->head));
                        }
                    }

                    state = sw_head_crlf;
                    priv->state = state;
                    
                    break;
                }
                
                break;
            }
            case sw_head_crlf:
            {
                if (ch == '\r')
                {
                    /* 遇到head与body分割的空白行了 */
                    state = sw_head_end;
                    priv->state = state;

                    break;
                }
                else if (ch != '\n')
                {
                    priv->head_key_start = pos;
                    state = sw_head_key;
                    priv->state = state;
                    
                    break;
                }

                break;
            }
            case sw_head_end:
            {
                if (ch != '\n')
                {
                    priv->body_start = pos;

                    state = sw_body;
                    priv->state = state;

                    break;
                }
                else
                {
                    if (request_msg->body_len == 0)
                    {
                        /* 无body数据，本则rtsp请求消息解析完毕 */
                        *parsed_bytes = priv->data_ptr_offset + 1;

                        return 0;
                    }
                }

                break;
            }
            case sw_body:
            {
                if ((pos - priv->body_start + 1) == request_msg->body_len)
                {
                    *parsed_bytes = priv->data_ptr_offset + 1;

                    /* body数据截取完毕，本则消息解析完成 */
                    request_msg->body = (char*)malloc(request_msg->body_len);
                    memcpy(request_msg->body, priv->body_start, request_msg->body_len);

                    return 0;
                }

                break;
            }
            default:
            {
                break;
            }
        }
    }
    
    return 1;
}

void rtsp_request_msg_destroy(rtsp_request_msg_t* request_msg)
{
    rtsp_head_t* head;
    
    if (NULL == request_msg)
    {
        return;
    }
    
    free(request_msg->url);
    request_msg->url = NULL;
    free(request_msg->body);
    request_msg->body = NULL;
    while (NULL != request_msg->head)
    {
        head = request_msg->head;
        request_msg->head = head->next;
        free(head);
    }
    free(request_msg);
    
    return;
}

rtsp_response_msg_t* rtsp_response_msg_new(void)
{
    rtsp_response_msg_t* response_msg;
    struct response_msg_private *priv;
    
    response_msg = (rtsp_response_msg_t*)malloc(sizeof(rtsp_response_msg_t) + sizeof(struct response_msg_private));
    memset(response_msg, 0, (sizeof(rtsp_response_msg_t) + sizeof(struct response_msg_private)));
    priv = (struct response_msg_private*)&response_msg[1];
    priv->ref_count = 1;
    priv->state = sw_start;

    return response_msg;
}

int rtsp_response_msg_decode(rtsp_response_msg_t* response_msg, const char *data, unsigned size, unsigned *parsed_bytes)
{
    struct response_msg_private *priv;
    
    const char *pos;
    const char *data_end;
    char ch;

    enum rtsp_msg_parse_state state;

    rtsp_head_t* head;

    *parsed_bytes = 0;

    priv = (struct response_msg_private*)&response_msg[1];
    data_end = data + size;

    state = priv->state;
    pos = data + priv->data_ptr_offset;
    for (; pos < data_end; ++pos)
    {
        priv->data_ptr_offset = pos - data;

        ch = *pos;
        switch (state)
        {
            case sw_start:
            {
                if (ch == '\r' || ch == '\n')
                {
                    break;
                }

                if (ch >= 'A' && ch <= 'Z')
                {
                    priv->version_start = pos;
                    state = sw_version;
                    priv->state = state;

                    break;
                }
                else
                {
                    log_error("rtsp_response_msg_decode: bad rtsp message start");
                    return -1;
                }

                break;
            }
            case sw_version:
            {
                if (ch == ' ')
                {
                    priv->version_end = pos-1;

                    if ((priv->version_end - priv->version_start) < 7)
                    {
                        log_error("rtsp_response_msg_decode: rtsp version string piece is too short");
                        return -1;
                    }

                    /* 检查RTSP版本信息 */
                    if (!ngx_str5cmp(priv->version_start, 'R', 'T', 'S', 'P', '/'))
                    {
                        log_error("rtsp_response_msg_decode: rtsp version is not started with 'RTSP/'");
                        return -1;
                    }
                    else
                    {
                        if (priv->version_start[5] < '0' || priv->version_start[5] > '9')
                        {
                            log_error("rtsp_response_msg_decode: bad rtsp main version number");
                            return -1;
                        }
                        else
                        {
                            response_msg->version |= (int)(priv->version_start[5] - '0') << 8;
                        }
                        
                        if (priv->version_start[6] != '.')
                        {
                            log_error("rtsp_response_msg_decode: bad rtsp version delimit");
                            return -1;
                        }

                        if (priv->version_start[7] < '0' || priv->version_start[7] > '9')
                        {
                            log_error("rtsp_response_msg_decode: bad rtsp minor version number");
                            return -1;
                        }
                        else
                        {
                            response_msg->version |= (int)(priv->version_start[7] - '0');
                        }
                    }

                    state = sw_space_after_version;
                    priv->state = state;

                    break;
                }

                break;
            }
            case sw_space_after_version:
            {
                if (ch == ' ')
                {
                    break;
                }

                priv->status_code_start = pos;
                state = sw_status_code;
                priv->state = state;

                break;
            }
            case sw_status_code:
            {
                if (ch == ' ')
                {
                    priv->status_code_end = pos - 1;

                    response_msg->code = atoi(priv->status_code_start);

                    state = sw_space_after_status_code;
                    priv->state = state;

                    break;
                }

                if (ch < '0' || ch > '9')
                {
                    /* status 中出现非ascii字符，为非法消息 */
                    log_error("rtsp_request_msg_decode: non-number char in status code");
                    return -1;
                }

                break;
            }
            case sw_space_after_status_code:
            {
                if (ch == ' ')
                {
                    break;
                }

                priv->status_text_start = pos;

                state = sw_status_text;
                priv->state = state;

                break;
            }
            case sw_status_text:
            {
                if (ch == '\r')
                {
                    priv->status_text_end = pos-1;

                    state = sw_rl_crlf;
                    priv->state = state;

                    break;
                }

                break;
            }
            case sw_rl_crlf:
            {
                if(ch != '\n')
                {
                    priv->head_key_start = pos;
                    state = sw_head_key;
                    priv->state = state;

                    break;
                }

                break;
            }
            case sw_head_key:
            {
                if (ch == ':')
                {
                    priv->head_key_end = pos - 1;
                    state = sw_head_colon;
                    priv->state = state;
                    
                    break;
                }

                break;
            }
            case sw_head_colon:
            {
                if (ch != ' ')
                {
                    priv->head_value_start = pos;
                    state = sw_head_value;
                    priv->state = state;
                    break;
                }

                break;
            }
            case sw_head_value:
            {
                if (ch == '\r')
                {
                    priv->head_value_end = pos - 1;

                    head = rtsp_msg_head_parse(priv->head_key_start, priv->head_key_end, priv->head_value_start, priv->head_value_end);
                    if (NULL != head)
                    {
                        if (head->key == RTSP_HEAD_CSEQ)
                        {
                            response_msg->cseq = atoi(head->value);
                            rtsp_msg_head_destroy(head);
                        }
                        else if (head->key == RTSP_HEAD_CONTENT_LENGTH)
                        {
                            response_msg->body_len = atoi(head->value);
                            rtsp_msg_head_destroy(head);
                        }
                        else
                        {
                            save_rtsp_head(head, &(response_msg->head));
                        }
                    }

                    priv->head_key_start = NULL;
                    priv->head_key_end = NULL;                    
                    priv->head_value_start = NULL;
                    priv->head_value_end = NULL;

                    state = sw_head_crlf;
                    priv->state = state;

                    break;
                }
                
                break;
            }
            case sw_head_crlf:
            {
                if (ch == '\r')
                {
                    /* 遇到head与body分割的空白行了 */
                    state = sw_head_end;
                    priv->state = state;

                    break;
                }
                else if (ch != '\n')
                {
                    priv->head_key_start = pos;
                    state = sw_head_key;
                    priv->state = state;

                    break;
                }

                break;
            }
            case sw_head_end:
            {
                if (ch != '\n')
                {
                    priv->body_start = pos;
                    
                    state = sw_body;
                    priv->state = state;

                    break;
                }
                else
                {
                    if (response_msg->body_len == 0)
                    {
                        /* 无body数据，本则rtsp请求消息解析完毕 */
                        *parsed_bytes = priv->data_ptr_offset + 1;

                        return 0;
                    }
                }

                break;
            }
            case sw_body:
            {
                if ((pos - priv->body_start + 1) == response_msg->body_len)
                {
                    *parsed_bytes = priv->data_ptr_offset + 1;

                    /* body数据截取完毕，本则消息解析完成 */
                    response_msg->body = (char*)malloc(response_msg->body_len);
                    memcpy(response_msg->body, priv->body_start, response_msg->body_len);

                    return 0;
                }
                
                break;
            }
            default:
            {
                break;
            }
        }
    }
    
    return 1;
}

void rtsp_response_msg_destroy(rtsp_response_msg_t* response_msg)
{
    rtsp_head_t *head;

    if (NULL == response_msg)
    {
        return;
    }

    while (NULL != response_msg->head)
    {
        head = response_msg->head;
        response_msg->head = head->next;
        free(head);
    }
    free(response_msg->body);
    response_msg->body = NULL;
    free(response_msg);

    return;
}

void rtsp_request_msg_ref(rtsp_request_msg_t* request_msg)
{
    struct request_msg_private *priv;

    if (NULL == request_msg)
    {
        return;
    }

    priv = (struct request_msg_private *)&request_msg[1];
    (void)atomic_inc(&priv->ref_count);

    return;
}

int rtsp_request_msg_unref(rtsp_request_msg_t* request_msg)
{
    int ref_count;
    struct request_msg_private *priv;

    if (NULL == request_msg)
    {
        return 0;
    }

    priv = (struct request_msg_private *)&request_msg[1];
    ref_count = atomic_dec(&priv->ref_count);   /* 返回的是旧值 */
    if (1 == ref_count)
    {
        rtsp_request_msg_destroy(request_msg);
    }

    return ref_count;
}

/** 增加一次引用计数 */
void rtsp_response_msg_ref(rtsp_response_msg_t* response_msg)
{
    struct response_msg_private *priv;

    if (NULL == response_msg)
    {
        return;
    }

    priv = (struct response_msg_private *)&response_msg[1];
    (void)atomic_inc(&priv->ref_count);

    return;
}

/** 减少一次引用计数，并返回操作后的计数值，当计数值为0时，该对象将被销毁 */
int rtsp_response_msg_unref(rtsp_response_msg_t* response_msg)
{
    int ref_count;
    struct response_msg_private *priv;

    if (NULL == response_msg)
    {
        return 0;
    }

    priv = (struct response_msg_private *)&response_msg[1];
    ref_count = atomic_dec(&priv->ref_count);
    if (1 == ref_count)
    {
        rtsp_response_msg_destroy(response_msg);
    }

    return ref_count-1;
}

int rtsp_msg_build_response
(
    char *response_msg, unsigned len, int cseq, int code, 
    rtsp_head_t* head, const char* body, unsigned body_len
)
{
    int total;
    int lentmp;
    time_t t;
    struct tm *tm;
    rtsp_head_t* node;

    if (NULL == response_msg || 0 == len)
    {
        log_error("rtsp_msg_build_response: bad response_msg buffer(%p) or bad len(%u)", 
                  response_msg, len);
        return 0;
    }

    t = time(NULL);
    tm = gmtime(&t);

    total = 0;
    total = snprintf(response_msg, len, 
                     "RTSP/1.0 %d %s\r\n"
                     "CSeq: %d\r\n"
                     "Server: tinylib/rtsp\r\n",
                     code, get_status_text(code), 
                     cseq);
    lentmp = strftime(response_msg+total, (len-total), "Date: %a, %b %d %Y %H:%M:%S GMT\r\n", tm);
    total += lentmp;

    while (NULL != head)
    {
        node = head->next;
        if (NULL != head->value)
        {
            lentmp = snprintf((response_msg+total), (len-total), "%s: %s\r\n", head_key_text[(int)head->key], head->value);
            total += lentmp;
        }

        head = node;
    }

    if (NULL != body && body_len > 0)
    {
        lentmp = snprintf((response_msg+total), (len-total), "Content-Length: %u\r\n", body_len);
        total += lentmp;        
    
        lentmp = snprintf((response_msg+total), (len-total), "\r\n");
        total += lentmp;

        strncpy((response_msg+total), body, body_len);
        total += body_len;
    }
    else
    {
        lentmp = snprintf((response_msg+total), (len-total), "\r\n");
        total += lentmp;    
    }

    return total;
}

int rtsp_msg_buid_request
(
    char* request_msg, unsigned len, int cseq, rtsp_method_e method, const char* url, 
    rtsp_head_t* head, const char* body, unsigned body_len
)
{
    int total;
    rtsp_head_t *node;
    int lentmp;
    
    int m;
    int m_idx;

    if (NULL == request_msg || 0 == len || NULL == url)
    {
        log_error("rtsp_msg_buid_request: bad request_msg(%p) or bad len(%u) or bad url(%p)", 
                  request_msg, len, method, url);
        return 0;
    }
    
    if (method == RTSP_METHOD_NONE)
    {
        return 0;
    }
    
    m_idx = 0;
    m =(int)method;
    while (m > 0)
    {
        m = m>>1;
        m_idx++;
    }

    total = 0;
    total = snprintf(request_msg, len,
                     "%s %s RTSP/1.0\r\n"
                     "CSeq: %u\r\n"
                     "User-Agent: tinylib/rtsp\r\n",
                     method_text[m_idx-1], 
                     url,
                     cseq);
    while (NULL != head)
    {
        node = head->next;

        if (NULL != head->value)
        {
            lentmp = snprintf(request_msg+total, (len-total), "%s: %s\r\n", head_key_text[(int)head->key], head->value);
            total += lentmp;
        }

        head = node;
    }

    if (NULL != body && 0 < body_len)
    {
        lentmp = snprintf((request_msg+total), (len-total), "Content-Length: %u\r\n", body_len);
        total += lentmp;        
    
        lentmp = snprintf((request_msg+total), (len-total), "\r\n");
        total += lentmp;
    
        strncpy((request_msg+total), body, body_len);
        total += body_len;    
    }
    else
    {
        lentmp = snprintf((request_msg+total), (len-total), "\r\n");
        total += lentmp;       
    }

    return total;
}

rtsp_transport_head_t* rtsp_transport_head_decode(const char *data)
{
    unsigned len;
    char *part;
    char *pos;
    char *pos1;
    char *cr;
    char *raw;
    rtsp_transport_head_t* transport_head;
    unsigned short rtp_port;
    unsigned short rtcp_port;
    unsigned char rtp_channel;
    unsigned char rtcp_channel;    

    if (NULL == data)
    {
        log_error("rtsp_transport_head_decode: bad data(%p)", data);
        return NULL;
    }

    len = strlen(data);

    /* 额外加1，是为了预留一个\0字符的位置 */
    transport_head = (rtsp_transport_head_t*)malloc(sizeof(rtsp_transport_head_t) + len + 1);
    memset(transport_head, 0, (sizeof(rtsp_transport_head_t) + len + 1));
    raw = (char*)transport_head + sizeof(rtsp_transport_head_t);
    strcpy(raw, data);

    part = raw;
    do
    {
        while (*part == ' ' || *part == '\t') part++;
        
        pos = strchr(part, ';');
        if (NULL == pos)
        {
            if (part == raw)
            {
                /* 整行中都没有分号,非法的transport head */
                free(transport_head);
                return NULL;
            }

            /* transport head中最后一个属性 */
            cr = strchr(part, '\r');
            if (NULL != cr)
            {
                *cr = '\0';
            }

            if (strstr(part, "RTP"))
            {
                transport_head->trans = part;
            }
            else if (strstr(part, "cast"))
            {
                transport_head->cast = part;
            }
            else if (strncasecmp(part, "client_port=", 12) == 0)
            {
                part += 12;  /* 12 -> strlen("client_port=") */
                rtp_port = rtcp_port = 0;

                pos1 = strchr(part, '-');
                if (NULL == pos1)
                {
                    /* 不是一个端口对 */
                }
                else
                {
                    *pos1 = '\0';
                    pos1++;

                    rtp_port = (unsigned short)atoi(part);
                    rtcp_port = (unsigned short)atoi(pos1); 
                }
                transport_head->client_rtp_port = rtp_port;
                transport_head->client_rtcp_port = rtcp_port;
            }
            else if (strncasecmp(part, "server_port=", 12) == 0)
            {
                part += 12;  /* 12 -> strlen("server_port=") */
                rtp_port = rtcp_port = 0;
                pos1 = strchr(part, '-');
                if (NULL == pos1)
                {
                    /* 不是一个端口对 */
                }
                else
                {
                    *pos1 = '\0';
                    pos1++;

                    rtp_port = (unsigned short)atoi(part);
                    rtcp_port = (unsigned short)atoi(pos1); 
                }
                transport_head->server_rtp_port = rtp_port;
                transport_head->server_rtcp_port = rtcp_port;
            }
            else if (strncasecmp(part, "destination=", 12) == 0)
            {
                part += 12; /*12 -> strlen("destination=") */
                transport_head->destination = part;
            }
            else if (strncasecmp(part, "source=", 7) == 0)
            {
                part += 7; /* 7 -> strlen("source=") */
                transport_head->source = part;
            }
            else if (strncasecmp(part, "ssrc=", 5) == 0)
            {
                part += 5; /* 5 -> strlen("ssrc=") */
                transport_head->ssrc = part;
            }
            else if (strncasecmp(part, "interleaved=", 12) == 0)
            {
                transport_head->interleaved = 1;
                part += 12;
                rtp_channel = rtcp_channel = 0;
                pos1 = strchr(part, '-');
                if (NULL == pos1)
                {
                    /* 不是一个channel对 */
                    transport_head->interleaved = 0;
                }
                else
                {
                    *pos1 = '\0';
                    pos1++;

                    rtp_channel = (unsigned char)atoi(part);
                    rtcp_channel = (unsigned char)atoi(pos1); 
                }
                transport_head->rtp_channel = rtp_channel;
                transport_head->rtcp_channel = rtcp_channel;                
            }

            break;
        }
        else
        {
            *pos = '\0';
            pos++;
            while (*pos == ' ' || *pos == '\t') pos++;

            if (strstr(part, "RTP"))
            {
                transport_head->trans = part;
            }
            else if (strstr(part, "cast"))
            {
                transport_head->cast = part;
            }
            else if (strncasecmp(part, "client_port=", 12) == 0)
            {
                part += 12;  /* 12 -> strlen("client_port=") */
                rtp_port = rtcp_port = 0;

                pos1 = strchr(part, '-');
                if (NULL == pos1)
                {
                    /* 不是一个端口对 */
                }
                else
                {
                    *pos1 = '\0';
                    pos1++;

                    rtp_port = (unsigned short)atoi(part);
                    rtcp_port = (unsigned short)atoi(pos1); 
                }
                transport_head->client_rtp_port = rtp_port;
                transport_head->client_rtcp_port = rtcp_port;
            }
            else if (strncasecmp(part, "server_port=", 12) == 0)
            {
                part += 12;  /* 12 -> strlen("server_port=") */
                rtp_port = rtcp_port = 0;

                pos1 = strchr(part, '-');
                if (NULL == pos1)
                {
                /* 不是一个端口对 */
                }
                else
                {
                    *pos1 = '\0';
                    pos1++;

                    rtp_port = (unsigned short)atoi(part);
                    rtcp_port = (unsigned short)atoi(pos1); 
                }
                transport_head->server_rtp_port = rtp_port;
                transport_head->server_rtcp_port = rtcp_port;
            }
            else if (strncasecmp(part, "destination=", 12) == 0)
            {
                part += 12; /*12 -> strlen("destination=") */
                transport_head->destination = part;
            }
            else if (strncasecmp(part, "source=", 7) == 0)
            {
                part += 7; /* 7 -> strlen("source=") */
                transport_head->source = part;
            }
            else if (strncasecmp(part, "ssrc=", 5) == 0)
            {
                part += 5; /* 5 -> strlen("ssrc=") */
                transport_head->ssrc = part;
            }
            else if (strncasecmp(part, "interleaved=", 12) == 0)
            {
                transport_head->interleaved = 1;
                part += 12;
                rtp_channel = rtcp_channel = 0;
                pos1 = strchr(part, '-');
                if (NULL == pos1)
                {
                    /* 不是一个channel对 */
                    transport_head->interleaved = 0;
                }
                else
                {
                    *pos1 = '\0';
                    pos1++;

                    rtp_channel = (unsigned char)atoi(part);
                    rtcp_channel = (unsigned char)atoi(pos1); 
                }
                transport_head->rtp_channel = rtp_channel;
                transport_head->rtcp_channel = rtcp_channel;                
            }

            part = pos;
            continue;
        }        
    }while((unsigned)(part - raw) < len);

    return transport_head;
}

void rtsp_transport_head_destroy(rtsp_transport_head_t* head)
{
    free(head);

    return;
}

/* 输入的字符串如下
  WWW-Authenticate: Digest realm="8ce748cedf4c", nonce="b80c3869d2adcaee62d0a60a233d722d", stale="FALSE"
  WWW-Authenticate: Basic realm="8ce748cedf4c"
  */
rtsp_authenticate_head_t* rtsp_authenticate_head_decode(const char *auth)
{
    rtsp_authenticate_head_t* auth_head;
    unsigned len;
    char *raw;

    char *part;
    char *pos;    

    if (NULL == auth)
    {
        return NULL;
    }

    len = strlen(auth);
    auth_head = (rtsp_authenticate_head_t*)malloc(sizeof(rtsp_authenticate_head_t) + len);
    memset(auth_head, 0, sizeof(rtsp_authenticate_head_t) + len + 1);
    raw = (char*)auth_head + sizeof(rtsp_authenticate_head_t);
    memcpy(raw, auth, len);

    part = raw;
    
    while (*part == ' ' || *part == '\t') part++;

    auth_head->type = part;
    
    pos = strchr(part, ' ');
    if (NULL == pos)
    {
        pos = strchr(part, '\t');
    }
    if (NULL == pos)
    {
        log_error("rtsp_authenticate_head_decode: \"%s\"is not a valid authenticate header", auth);
        free(auth_head);
        return NULL;
    }

    *pos = '\0';

    pos++;
    part = pos;
    do 
    {
        while (*part == ' ' || *part == '\t') part++;

        if (strstr(part, "realm=\"") != NULL)
        {
            part += 7;      /* 7 -> strlen("realm=\"") */
            auth_head->realm = part;
        }
        else if (strstr(part, "nonce=\"") != NULL)
        {
            part += 7;      /* 7 -> strlen("nonce=\"") */
            auth_head->nonce = part;
        }
        else if (strstr(part, "stale=\""))
        {
            part += 7;      /* 7 -> strlen("stale=\"") */
            auth_head->stale = part;
        }

        pos = strchr(part, '"');
        if (NULL != pos)
        {
            *pos = '\0';
            part = pos+1;
        }
        else
        {
            /* 认为到行尾，结束解析 */
            break;
        }
        
        pos = strchr(part, ',');
        if (NULL != pos)
        {
            pos++;
            part = pos;
        }
        else
        {
            break;
        }
    } while((unsigned)(part - raw) < len);

    return auth_head;
}


void rtsp_authenticate_head_destroy(rtsp_authenticate_head_t* head)
{
    free(head);

    return;
}

/* 将给定的数据转换成16进制文本显示格式 */
static inline 
void hex_text(const unsigned char *data, unsigned size, char *text)
{
    unsigned i;
    static char hex[16] = "0123456789abcdef";

    for(i = 0; i < size; ++i)
    {
        text[i+i] = hex[data[i]>>4];
        text[i+i+1] = hex[data[i]& 0x0f];
    }

    return;
}

/* 产生的头类似于如下内容
Digest username="admin", realm="8ce748cedf4c", nonce="b80c3869d2adcaee62d0a60a233d722d", uri="rtsp://10.10.8.33", response="e0e5638a0a5b3b0e8bb61ff0b4996618"
 */
unsigned rtsp_authorization_head
(
    char *auth, unsigned len, const char *user, const char *password, 
    const char *method, const char *url, const char *realm, const char *nonce
)
{
    char ha1buf[512];
    unsigned ha1len;
    unsigned char ha1digest[MD5_DIGEST_LENGTH];
    char ha1digest_text[33];
    
    char ha2buf[512];
    unsigned ha2len;
    unsigned char ha2digest[MD5_DIGEST_LENGTH];
    char ha2digest_text[33];

    char response_buf[512];
    unsigned response_len;
    unsigned char response_digest[MD5_DIGEST_LENGTH];
    char response_text[33];

    unsigned total;

    if (NULL == auth || 0 == len || NULL == user || NULL == password || NULL == method || NULL == url || NULL == realm || NULL == nonce)
    {
        log_error("rtsp_authorization_head: bad auth(%p) or bad len(%u) or bad user(%p) or bad password(%p) or bad method(%p) or "
                  "bad url(%p) or bad realm(%p) or bad once(%p)", 
                  auth, len, user, password, method, url, realm, nonce);

        return 0;
    }

    /*
     ha1digest = md5(user:realm:pass)
     ha2digest = md5(method:url)
     response = md5(ha1digest:nonce:ha2digest)
    */

    ha1len = 0;
    memset(ha1buf, 0, sizeof(ha1buf));
    ha1len = snprintf(ha1buf, sizeof(ha1buf), "%s:%s:%s", user, realm, password);
    memset(ha1digest, 0, sizeof(ha1digest));
    MD5((unsigned char*)ha1buf, ha1len, ha1digest);
    memset(ha1digest_text, 0, sizeof(ha1digest_text));
    hex_text(ha1digest, MD5_DIGEST_LENGTH, ha1digest_text);

    ha2len = 0;
    memset(ha2buf, 0, sizeof(ha2buf));
    ha2len = snprintf(ha2buf, sizeof(ha2buf), "%s:%s", method, url);
    memset(ha2digest, 0, sizeof(ha2digest));
    MD5((unsigned char*)ha2buf, ha2len, ha2digest);
    memset(ha2digest_text, 0, sizeof(ha2digest_text));
    hex_text(ha2digest, MD5_DIGEST_LENGTH, ha2digest_text);

    response_len = 0;
    memset(response_buf, 0, sizeof(response_buf));
    response_len = snprintf(response_buf, sizeof(response_buf), "%s:%s:%s", ha1digest_text, nonce, ha2digest_text);
    MD5((unsigned char*)response_buf, response_len, response_digest);
    memset(response_text, 0, sizeof(response_text));
    hex_text(response_digest, MD5_DIGEST_LENGTH, response_text);

    total = snprintf(auth, len, "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", response=\"%s\"",
        user, realm, nonce, url, response_text);

    return total;
}
