
#include "tinylib/rtsp/sdp.h"

#include <string.h>
#include <assert.h>
#include <stdio.h>

#if 0
static char sdpdata[] = "v=0\r\n"
"o=- 1408281261003656 1408281261003656 IN IP4 10.10.8.33\r\n"
"s=Media Presentation\r\n"
"e=NONE\r\n"
"b=AS:5050\r\n"
"t=0 0\r\n"
"a=control:rtsp://10.10.8.33:554/\r\n"
"m=video 0 RTP/AVP 96\r\n"
"b=AS:5000\r\n"
"a=rtpmap:96 H264/90000\r\n"
"a=fmtp:96 profile-level-id=420029; packetization-mode=1; sprop-parameter-sets=Z00AH5pmAoAt/zUBAQFAAAD6AAAw1AE=,aO48gA==\r\n"
"a=control:rtsp://10.10.8.33:554/trackID=1\r\n"
"m=application 0 RTP/AVP 107\r\n"
"b=AS:50\r\n"
"a=rtpmap:107 vnd.onvif.metadata/90000\r\n"
"a=Media_header:MEDIAINFO=494D4B48010100000400010000000000000000000000000000000000000000000000000000000000;\r\n"
"a=appversion:1.0\r\n"
"a=control:rtsp://10.10.8.33:554/trackID=3\r\n";
#endif

static char sdpdata1[] = \
"v=0\r\n"
"o=- 0 0 IN IP4 10.10.3.102\r\n"
"s=Unnamed\r\n"
"i=N/A\r\n"
"c=IN IP4 0.0.0.0\r\n"
"t=0 0\r\n"
"m=audio 0 RTP/AVP 96\r\n"
"a=rtpmap:96 AMR/8000\r\n"
"a=fmtp:96 octet-align=1;\r\n"
"a=control:rtsp://10.10.3.102:7711/?puid=1000/trackID=0\r\n"
"a=recvonly\r\n"
"m=video 0 RTP/AVP 96\r\n"
"a=rtpmap:96 H264/90000\r\n"
"a=fmtp:96 packetization-mode=1;profile-level-id=424029;sprop-parameter-sets=J0JAKYuVAUB7IA==,KN4JiA==\r\n"
"a=control:rtsp://10.10.3.102:7711/?puid=1000/trackID=1\r\n";

#if 0
static char sdpdata2[] = \
"v=0\r\n"
"o=- 1409911579497384 1409911579497384 IN IP4 10.10.8.53\r\n"
"s=Media Presentation\r\n"
"e=NONE\r\n"
"b=AS:5100\r\n"
"t=0 0\r\n"
"a=control:rtsp://10.10.8.53:554/\r\n"

"m=video 0 RTP/AVP 96\r\n"
"b=AS:5000\r\n"
"a=control:rtsp://10.10.8.53:554/trackID=1\r\n"
"a=rtpmap:96 H264/90000\r\n"
"a=fmtp:96 profile-level-id=420029; packetization-mode=1; sprop-parameter-sets=Z01AHppmBQHv81AQEBQAAA+gAAHUwBA=,aO48gA==\r\n"

"m=audio 0 RTP/AVP 0\r\n"
"b=AS:50\r\n"
"a=control:rtsp://10.10.8.53:554/trackID=2\r\n"
"a=rtpmap:0 PCMU/8000\r\n"
"a=Media_header:MEDIAINFO=494D4B48010100000400010010710110401F000000FA000000000000000000000000000000000000;\r\n"
"a=appversion:1.0\r\n";
#endif

int main()
{
    sdp_t *sdp;
    sdp_session_t *session;
    sdp_media_t *media;
    int i;

    const sdp_attrib_t *attrib;
    const sdp_attrib_t *a_iter;

    sdp = sdp_parse(sdpdata1, strlen(sdpdata1));
    assert(sdp);

    session = sdp->session;

    printf("Session:\n"
    "version => %s\n"
    "origin_user => %s\n"
    "origin_session_id => %s\n"
    "origin_session_ver => %s\n"
    "origin_net_type => %s\n"
    "origin_addr_type => %s\n"
    "origin_addr => %s\n"
    "name => %s\n"
    "email => %s\n"
    "bandwidth => %s\n"
    "time => %s\n"
    "control => %s\n",
    session->version, 
    session->origin_user, session->origin_session_id, session->origin_session_ver, session->origin_net_type, 
    session->origin_addr_type, session->origin_addr,
    session->name, session->email, session->bandwidth, session->time, session->control);
    attrib = session->attrib;
    while (NULL != attrib)
    {
        a_iter = attrib->next;
        printf("a %s => %s\n", attrib->key, attrib->value);
        attrib = a_iter;
    }

    for (i = 0; i < 3; ++i)
    {
        media = sdp->media[i];
        if (NULL == media)
        {
            continue;
        }
    
        printf("Media:\n"
        "type => %s\n"
        "param => %s\n"
        "bandwidth => %s\n"
        "control => %s\n",
        media->type, media->param, media->bandwidth, media->control);

        attrib = media->attrib;
        while (NULL != attrib)
        {
            a_iter = attrib->next;
            printf("a %s => %s\n", attrib->key, attrib->value);
            attrib = a_iter;
        }
    }

    sdp_destroy(sdp);

    return 0;
}

