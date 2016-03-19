
/* 一个SDP的一个基本解析器，只做简单的文本解析，
 * 具体的值含义，请使用者根据需要自行做进一步解析
 */

#ifndef RTSP_SDP_H
#define RTSP_SDP_H

typedef struct sdp_attrib
{
    const char* key;
    const char* value;
    const struct sdp_attrib *next;
}sdp_attrib_t;

/* sdp信息中session部分的记录 */
typedef struct sdp_session
{
    const char* version;
    const char* origin_user;
    const char* origin_session_id;
    const char* origin_session_ver;
    const char* origin_net_type;
    const char* origin_addr_type;
    const char* origin_addr;
    const char* name;
    const char* email;
    const char* bandwidth;
    const char* time;
    const char* control;
    
    const sdp_attrib_t *attrib;
    const sdp_attrib_t *attrib_end;
}sdp_session_t;

typedef struct sdp_media
{
    const char* type;                /* video/audio */
    const char* param;               /*m=vedio/audio之后的内容 */
    const char* bandwidth;
    const char* control; 
    const sdp_attrib_t* attrib;      /* 除control属性之外其他的attribute */
    const sdp_attrib_t *attrib_end;
}sdp_media_t;

typedef struct sdp
{
    sdp_session_t *session;
    sdp_media_t *media[3];      /* 目前最多解析三个子媒体流分别使用tackID进行标示 */
}sdp_t;

#ifdef __cplusplus
extern "C" {
#endif

sdp_t* sdp_parse(const char *data, unsigned len);

void sdp_destroy(sdp_t* sdp);

#ifdef __cplusplus
}
#endif

#endif /* !RTSP_SDP_H */
