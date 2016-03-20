
#include "tinylib/rtsp/rtsp_server.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifdef WINNT
    #include <winsock2.h>
#endif

static loop_t *g_loop = NULL;

static 
void session_hander
(
    rtsp_session_t* session, 
    tcp_connection_t* connection, 
    rtsp_request_msg_t *request_msg, 
    void *userdata
)
{
    char response[1024];
    size_t size;
    rtsp_head_t *head;
    rtsp_head_t *node;

    rtsp_head_t public_head;

    rtsp_transport_head_t *transport;
    rtsp_head_t content_type_head;
    
    rtsp_head_t transport_head;
    rtsp_head_t session_head;
    const char *body;
    size_t body_len;

    if (NULL == request_msg)
    {
        if (tcp_connection_connected(connection) == 0)
        {
            rtsp_session_end(session);
            return;
        }

        return;
    }
    
    printf("new request arrived:\n"
        "method: %d\n"
        "url: %s\n"
        "version: 0x%x\n"
        "cseq: %d\n",
        request_msg->method, request_msg->url, request_msg->version, request_msg->cseq);

    if (NULL != request_msg->head)
    {
        head = request_msg->head;
        while (NULL != head)
        {
            node = head->next;
            printf("%d => %s\n", head->key, head->value);
            if (head->key == RTSP_HEAD_TRANSPORT)
            {
                transport = rtsp_transport_head_decode(head->value);
                printf("\tmode: %s\n"
                       "\tcast: %s\n"
                       "\tdestination: %s\n"
                       "\tsource: %s\n"
                       "\tclient_rtp: %u\n"
                       "\tclient_rtcp: %u\n"
                       "\tserver_rtp: %u\n"
                       "\tserver_rcp: %u\n"
                       "\tssrc: %s\n"
                       "\tinterleaved: %d\n"
                       "\trtp_channel: %d\n"
                       "\trtcp_channel: %d\n", 
                    transport->trans,
                    transport->cast,
                    transport->destination,
                    transport->source,
                    transport->client_rtp_port,
                    transport->client_rtcp_port,
                    transport->server_rtp_port,
                    transport->server_rtcp_port,
                    transport->ssrc,
                    transport->interleaved,
                    transport->rtp_channel,
                    transport->rtcp_channel);
                rtsp_transport_head_destroy(transport);
            }
            
            head = node;
        }
    }

    if (request_msg->body != NULL && request_msg->body_len > 0)
    {
        printf("body len: %u\n", request_msg->body_len);
    }

    printf("========================================\n");

    head = NULL;
    body = NULL;
    body_len = 0;
    if (RTSP_METHOD_OPTIONS == request_msg->method)
    {
        public_head.key = RTSP_HEAD_PUBLIC;
        public_head.value = "OPTIONS, DESCRIBE, SETUP, PLAY, TEARDOWN";
        public_head.next = NULL;
        head = &public_head;
        body = NULL;
        body_len = 0;        
    }
    else if (RTSP_METHOD_DESCRIBE == request_msg->method)
    {
        content_type_head.key = RTSP_HEAD_CONTENT_TYPE;
        content_type_head.value = "application/sdp";
        content_type_head.next = NULL;
        head = &content_type_head;
    
        body = "v=0\r\n"
               "o=- 1408281261003656 1408281261003656 IN IP4 127.0.0.1\r\n"
               "s=Media Presentation\r\n"
               "e=NONE\r\n"
               "b=AS:5050\r\n"
               "t=0 0\r\n"
               "a=control:rtsp://127.0.0.1:554/\r\n"
               "m=video 0 RTP/AVP 96\r\n"
               "b=AS:5000\r\n"
               "a=control:rtsp://127.0.0.1:554/trackID=1\r\n"
               "a=rtpmap:96 H264/90000\r\n"
               "a=fmtp:96 profile-level-id=420029; packetization-mode=1; sprop-parameter-sets=Z00AH5pmAoAt/zUBAQFAAAD6AAAw1AE=,aO48gA==,\r\n";
        body_len = strlen(body);
    }
    else if (RTSP_METHOD_SETUP == request_msg->method)
    {
        transport_head.key = RTSP_HEAD_TRANSPORT;
        /* transport_head.value = "RTP/AVP;unicast;destination=192.168.0.1;source=192.168.0.2;client_port=61222-61223;server_port=6970-6971"; */
        transport_head.value = "RTP/AVP/TCP;unicast;interleaved=0-1";
        transport_head.next = NULL;

        session_head.key = RTSP_HEAD_SESSION;
        session_head.value = "9580441;timeout=1";
        session_head.next = NULL;

        transport_head.next = &session_head;
        head = &transport_head;
        body = NULL;
        body_len = 0;
    }
        
    size = rtsp_msg_build_response(response, sizeof(response)-1, request_msg->cseq, 200, head, body, body_len);
    tcp_connection_send(connection, response, size);

    printf("\nResponse: \n%s\n", response);

    if (RTSP_METHOD_TEARDOWN == request_msg->method)
    {
        /* nothing to do */
    }

    return;
}

static
void interleaved_sink
(
    rtsp_session_t* session, 
    unsigned char channel,
    void* packet, unsigned short size,
    void *userdata
)
{
    printf("interleaved packet: channel: %u, size: %u\n\n", channel, size);
    
    return;
}

int main()
{
    rtsp_server_t *server;

    #ifdef WINNT
    WSADATA wsa_data;
    WSAStartup(MAKEWORD(2, 2), &wsa_data);
    #endif  
    
    g_loop = loop_new(1);
    assert(g_loop);
    server = rtsp_server_new(g_loop, session_hander, interleaved_sink, NULL, 554, "0.0.0.0");
    assert(server);
    rtsp_server_start(server);

    loop_loop(g_loop);

    rtsp_server_stop(server);
    rtsp_server_destroy(server);
    loop_destroy(g_loop);

    #ifdef WINNT
    WSACleanup();
    #endif    

    return 0;
}
