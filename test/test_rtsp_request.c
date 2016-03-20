
#include "tinylib/rtsp/rtsp_request.h"
#include "tinylib/rtsp/sdp.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef WINNT
#include <winsock2.h>
#endif

static loop_t *g_loop = NULL;
static rtsp_request_t *g_request = NULL;

static void onexpire(void* userdata)
{
    rtsp_request_teardown(g_request, NULL);
    
    return;
}

static void request_handler
(
    rtsp_request_t *request, 
    tcp_connection_t* connection, 
    rtsp_method_e method, 
    const rtsp_response_msg_t* response, 
    void *userdata
)
{
    rtsp_head_t head;
    const rtsp_head_t *iter;
    const rtsp_head_t *node;
    rtsp_transport_head_t *trans;
    sdp_t *sdp;
    sdp_media_t *sdp_media;
    int i;

    if (tcp_connection_connected(connection) == 0)
    {
        loop_quit(g_loop);
        return;
    }

    if (RTSP_METHOD_NONE == method)
    {
        rtsp_request_options(request);
        return;
    }

    printf("response received: \n"
           "cseq: %d, method: %d\n"
           "code: %d\n",
           response->cseq, method,
           response->code);
    iter = response->head;
    while (NULL != iter)
    {
        node = iter->next;
        printf("%d => %s\n", iter->key, iter->value);
        if (iter->key == RTSP_HEAD_TRANSPORT)
        {
            trans = rtsp_transport_head_decode(iter->value);
            printf("\ttrans: %s\n"
                   "\tcast: %s\n"
                   "\tdestination: %s\n"
                   "\tsource: %s\n"
                   "\tclient_rtp: %u\n"
                   "\tclient_rtcp: %u\n"
                   "\tserver_rtp: %u\n"
                   "\tserver_rtcp: %u\n"
                   "\tssrc: %s\n"
                   "\tinterleaved: %d\n"
                   "\trtp_channel: %d\n"
                   "\trtcp_channel: %d\n", 
                   trans->trans, 
                   trans->cast, 
                   trans->destination, 
                   trans->source, 
                   trans->client_rtp_port, 
                   trans->client_rtcp_port, 
                   trans->server_rtp_port, 
                   trans->server_rtcp_port,
                   trans->ssrc,
                   trans->interleaved,
                   trans->rtp_channel,
                   trans->rtcp_channel);
            rtsp_transport_head_destroy(trans);
        }

        iter = node;
    }

    if (NULL != response->body && response->body_len > 0)
    {
        printf("body:\n%s\n\n", response->body);
    }
    printf("========================================\n");

    if (RTSP_METHOD_OPTIONS == method)
    {
        head.key = RTSP_HEAD_ACCEPT;
        head.value = "application/sdp";
        head.next = NULL;
        rtsp_request_describe(request, &head);
    }
    else if (RTSP_METHOD_DESCRIBE == method)
    {
        head.key = RTSP_HEAD_TRANSPORT;
        /* head.value = "RTP/AVP;unicast;client_port=61222-61223"; */
        head.value = "RTP/AVP/TCP;unicast;interleaved=0-1";
        head.next = NULL;

        sdp = sdp_parse(response->body, response->body_len);

        for (i = 0; i < 3; ++i)
        {
            sdp_media = sdp->media[i];
            if (NULL != sdp_media)
            {
                if (strncmp(sdp_media->type, "video", 5) == 0)
                {
                    rtsp_request_setup(request, &head, sdp_media->control);
                    sdp_destroy(sdp);
                    return;
                }
            }
        }
        
        sdp_destroy(sdp);

        rtsp_request_setup(request, &head, NULL);
    }
    else if (RTSP_METHOD_SETUP == method)
    {
        rtsp_request_play(request, NULL);
    }
    else if (RTSP_METHOD_PLAY == method)
    {
        /* 此处只是测试rtsp协议流程，所以直接teardown了 */
        /* rtsp_request_teardown(request, NULL); */
        loop_runafter(g_loop, 2 * 1000, onexpire, NULL);
    }
    else if (RTSP_METHOD_TEARDOWN == method)
    {
    }

    return;
}

void interleaved_sink
(
    rtsp_request_t *request, 
    unsigned char channel,
    void* packet, unsigned short size,
    void *userdata
)
{
    printf("interleaved packet: channel: %u, size: %u\n", channel, size);
    
    return;
}

int main(int argc, char* argv[])
{
    const char *url;

    #ifdef WINNT
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif    

    if (argc > 1)
    {
        url = argv[1];
    }
    else
    {
        url = "rtsp://127.0.0.1:554/demo.mp4";
    }

    g_loop = loop_new(1);
    g_request = rtsp_request_new(g_loop, url, request_handler, interleaved_sink, NULL);

    rtsp_request_launch(g_request);

    loop_loop(g_loop);
    
    rtsp_request_destroy(g_request);
    loop_destroy(g_loop);

    #ifdef WINNT
    WSACleanup();
    #endif

    return 0;
}

