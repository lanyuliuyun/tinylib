
#ifndef RTP_RTCP_PACKET_H
#define RTP_RTCP_PACKET_H

#include <stdint.h>

typedef enum rtcp_packet_type{
    RTCP_PACKET_TYPE_SR = 200,
    RTCP_PACKET_TYPE_RR = 201,
    RTCP_PACKET_TYPE_SDES = 202,
    RTCP_PACKET_TYPE_BYE = 203,
    RTCP_PACKET_TYPE_APP = 204,
}rtcp_packet_type_e;

#ifndef USE_BIG_ENDIAN
typedef struct rtp_head
{
    uint16_t CC:4;
    uint16_t X:1;
    uint16_t P:1;
    uint16_t V:2;
    
    uint16_t PT:7;
    uint16_t M:1;            
    
    uint16_t SN;
    
    uint32_t timestamp;
    uint32_t ssrc;
}rtp_head_t;

typedef struct rtcp_head
{
    uint16_t RC:5;
    uint16_t P:1;
    uint16_t V:2;
    uint16_t PT:8;

    uint16_t length;
}rtcp_head_t;

typedef struct rtcp_receiver_report
{
    uint32_t ssrc;
    uint32_t identifier;
    
    uint32_t cumulative_lost:24;
    uint32_t fraction_lost:8;

    uint32_t sn_cycles_count:16;
    uint32_t highes_sn_recved:16;

    uint32_t interarrival_jitter;
    uint32_t last_sr_ts;
    uint32_t delay_since_last_sr;
}rtcp_receiver_report_t;

#else

typedef struct rtp_head
{
    uint16_t V:2;
    uint16_t P:1;
    uint16_t X:1;
    uint16_t CC:4;
    uint16_t M:1;
    uint16_t PT:7;

    uint16_t SN;

    uint32_t timestamp;
    uint32_t ssrc;
}rtp_head_t;

typedef struct rtcp_head
{
    uint16_t V:2;
    uint16_t P:1;
    uint16_t RC:5;
    uint16_t PT:8;

    uint16_t length;
}rtcp_head_t;

typedef struct rtcp_receiver_report
{
    uint32_t ssrc;
    uint32_t identifier;
    uint32_t fraction_lost:8;
    uint32_t cumulative_lost:24;
    
    uint32_t sn_cycles_count:16;
    uint32_t highest_sn_recved:16;

    uint32_t interarrival_jitter;
    uint32_t last_sr_ts;
    uint32_t delay_since_last_sr;
}rtcp_receiver_report_t;

#endif

typedef struct rtp_ext_head
{
    uint16_t profile_define;
    uint16_t dword_count;        /* 4字节的计数，不包括此头自身 */
}rtp_ext_head_t;

typedef struct rtcp_sender_report
{
    uint32_t ntp_ts_msw;
    uint32_t ntp_ts_lsw;
    uint32_t rtp_ts;
    uint32_t packet_count;
    uint32_t bytes_count;
}rtcp_sender_report_t;

#endif /* !RTP_RTCP_PACKET_H */
