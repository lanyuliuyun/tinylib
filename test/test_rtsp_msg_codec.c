
const char* rtsp_request_msgs[] = {
    "OPTIONS rtsp://127.0.0.1:8554 RTSP/1.0\r\n"
    "CSeq: 1\r\n"
    "User-Agent: RealMedia Player Version 6.0.9.1235 (linux-2.0-libc6-i386-gcc2.95)\r\n"
    "\r\n",

    "DESCRIBE rtsp://127.0.0.1:8554/home.mp3 RTSP/1.0\r\n"
    "CSeq: 2\r\n"
    "Accept: application/sdp\r\n"
    "\r\n",

    "DESCRIBE rtsp://127.0.0.1:8554/home.mp3 RTSP/1.0\r\n"
    "CSeq: 1\r\n"
    "Accept: application/sdp\r\n"
    "User-Agent: MPlayer (LIVE555 Streaming Media v2010.01.22)\r\n"
    "\r\n",

    "SETUP rtsp://127.0.0.1:8554/home.mp3/track1 RTSP/1.0\r\n"
    "CSeq: 2\r\n"
    "Transport: RTP/AVP;unicast;client_port=61222-61223\r\n"
    "User-Agent: MPlayer (LIVE555 Streaming Media v2010.01.22)\r\n"
    "\r\n",

    "PLAY rtsp://127.0.0.1:8554/home.mp3/ RTSP/1.0\r\n"
    "CSeq: 3\r\n"
    "Session: 00003633\r\n"
    "Range: npt=0.000-\r\n"
    "User-Agent: MPlayer (LIVE555 Streaming Media v2010.01.22)\r\n"
    "\r\n",

    "TEARDOWN rtsp://example.com/fizzle/foo RTSP/1.0\r\n"
    "CSeq: 4\r\n"
    "Session: 00003633\r\n"
    "User-Agent: MPlayer (LIVE555 Streaming Media v2010.01.22)\r\n"
    "\r\n"
};

const char* rtsp_response_msgs[] = {
    "RTSP/1.0 200 OK\r\n"
    "CSeq: 1\r\n"
    "Date: Sun, Aug 03 2014 08:00:47 GMT\r\n"
    "Public: OPTIONS, DESCRIBE, SETUP, TEARDOWN, PLAY, PAUSE, GET_PARAMETER, SET_PARAMETER\r\n"
    "\r\n",

    "RTSP/1.0 200 OK\r\n"
    "CSeq: 2\r\n"
    "Date: Sun, Aug 03 2014 08:00:47 GMT\r\n"
    "Content-Base: rtsp://127.0.0.1:8554/home.mp3/\r\n"
    "Content-Type: application/sdp\r\n"
    "Content-Length: 388\r\n"
    "\r\n"
    "v=0\r\n"
    "o=- 1407052847060393 1 IN IP4 10.0.0.2\r\n"
    "s=MPEG-1 or 2 Audio, streamed by the LIVE555 Media Server\r\n"
    "i=home.mp3\r\n"
    "t=0 0\r\n"
    "a=tool:LIVE555 Streaming Media v2011.03.14\r\n"
    "a=type:broadcast\r\n"
    "a=control:*\r\n"
    "a=range:npt=0-258.451\r\n"
    "a=x-qt-text-nam:MPEG-1 or 2 Audio, streamed by the LIVE555 Media Server\r\n"
    "a=x-qt-text-inf:home.mp3\r\n"
    "m=audio 0 RTP/AVP 14\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "b=AS:320\r\n"
    "a=control:track1\r\n",

    "RTSP/1.0 200 OK\r\n"
    "CSeq: 1\r\n"
    "Date: Sun, Aug 03 2014 08:00:47 GMT\r\n"
    "Content-Base: rtsp://127.0.0.1:8554/home.mp3/\r\n"
    "Content-Type: application/sdp\r\n"
    "Content-Length: 388\r\n"
    "\r\n"
    "v=0\r\n"
    "o=- 1407052847060393 1 IN IP4 10.0.0.2\r\n"
    "s=MPEG-1 or 2 Audio, streamed by the LIVE555 Media Server\r\n"
    "i=home.mp3\r\n"
    "t=0 0\r\n"
    "a=tool:LIVE555 Streaming Media v2011.03.14\r\n"
    "a=type:broadcast\r\n"
    "a=control:*\r\n"
    "a=range:npt=0-258.451\r\n"
    "a=x-qt-text-nam:MPEG-1 or 2 Audio, streamed by the LIVE555 Media Server\r\n"
    "a=x-qt-text-inf:home.mp3\r\n"
    "m=audio 0 RTP/AVP 14\r\n"
    "c=IN IP4 0.0.0.0\r\n"
    "b=AS:320\r\n"
    "a=control:track1\r\n",

    "RTSP/1.0 200 OK\r\n"
    "CSeq: 2\r\n"
    "Date: Sun, Aug 03 2014 08:00:47 GMT\r\n"
    "Transport: RTP/AVP;unicast;destination=127.0.0.1;source=127.0.0.1;client_port=61222-61223;server_port=6970-6971\r\n"
    "Session: 00003633\r\n"
    "\r\n",

    "RTSP/1.0 200 OK\r\n"
    "CSeq: 3\r\n"
    "Date: Sun, Aug 03 2014 08:00:47 GMT\r\n"
    "Range: npt=0.000-\r\n"
    "Session: 00003633\r\n"
    "RTP-Info: url=rtsp://127.0.0.1:8554/home.mp3/track1;seq=14222;rtptime=19516\r\n"
    "\r\n"
};

#include "tinylib/rtsp/rtsp_message_codec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char *argv[])
{
    rtsp_request_msg_t *request_msg;
    rtsp_response_msg_t *response_msg;
    const rtsp_head_t *head;
    const rtsp_head_t *node;
    rtsp_transport_head_t *trans;
    
    unsigned parsed_bytes;

    int i;

    for (i = 0; i < (sizeof(rtsp_request_msgs)/sizeof(rtsp_request_msgs[0])); ++i)
    {
        request_msg = rtsp_request_msg_new();
        (void)rtsp_request_msg_decode(request_msg, rtsp_request_msgs[i], strlen(rtsp_request_msgs[i]), &parsed_bytes);

        printf("Raw Msg:\n%s\n\n", rtsp_request_msgs[i]);
        printf(
            "Decoded:\n"
            "CSeq => %d\n"
            "method => %d\n"
            "url => %s\n"
            "version => 0x%x\n",
            request_msg->cseq,
            request_msg->method,
            request_msg->url,
            request_msg->version
        );
        if (NULL != request_msg->head)
        {
            head = request_msg->head;
            while(head)
            {
                node = head->next;
                printf("%d => %s\n", head->key, head->value);
                if (head->key == RTSP_HEAD_TRANSPORT)
                {
                    trans = rtsp_transport_head_decode(head->value);
                    printf(
                        "\tmode => %s\n"
                        "\tcast => %s\n"
                        "\tdestination => %s\n"
                        "\tsource => %s\n"
                        "\tclient_rtp => %u\n"
                        "\tclient_rtcp => %u\n"
                        "\tserver_rtp => %u\n"
                        "\tserver_rtcp => %u\n",
                        trans->trans,
                        trans->cast,
                        trans->destination,
                        trans->source,
                        trans->client_rtp_port,
                        trans->client_rtcp_port,
                        trans->server_rtp_port,
                        trans->server_rtcp_port
                    );
                    rtsp_transport_head_destroy(trans);
                }
                
                head = node;
            }
        }

        if (NULL != request_msg->body && request_msg->body_len > 0)
        {
            printf("body len => %d\n", request_msg->body_len);
        }
        
        printf("\n\n==============================\n\n");
        rtsp_request_msg_destroy(request_msg);
    }

    printf("\n\n==============================\n\n");

    for (i = 0; i < (sizeof(rtsp_response_msgs)/sizeof(rtsp_response_msgs[0])); ++i)
    {
        response_msg = rtsp_response_msg_new();
        
        (void)rtsp_response_msg_decode(response_msg, rtsp_response_msgs[i], strlen(rtsp_response_msgs[i]), &parsed_bytes);
        printf("Raw Msg:\n%s\n\n", rtsp_response_msgs[i]);
        printf(
            "Decoded:\n"
            "CSeq => %d\n"
            "code => %d\n",
            response_msg->cseq,
            response_msg->code
        );
        if (NULL != response_msg->head)
        {
            head = response_msg->head;
            while(head)
            {
                node = head->next;
                printf("%d => %s\n", head->key, head->value);
                if (head->key == RTSP_HEAD_TRANSPORT)
                {
                    trans = rtsp_transport_head_decode(head->value);
                    printf(
                        "\tmode => %s\n"
                        "\tcast => %s\n"
                        "\tdestination => %s\n"
                        "\tsource => %s\n"
                        "\tclient_rtp => %u\n"
                        "\tclient_rtcp => %u\n"
                        "\tserver_rtp => %u\n"
                        "\tserver_rtcp => %u\n",
                        trans->trans,
                        trans->cast,
                        trans->destination,
                        trans->source,
                        trans->client_rtp_port,
                        trans->client_rtcp_port,
                        trans->server_rtp_port,
                        trans->server_rtcp_port
                    );
                    rtsp_transport_head_destroy(trans);
                }

                head = node;                
            }
        }

        if (NULL != response_msg->body && response_msg->body_len > 0)
        {
            printf("body len => %d\n", response_msg->body_len);
        }
        
        printf("\n\n==============================\n\n");
        rtsp_response_msg_destroy(response_msg);

    }

    return 0;
}
