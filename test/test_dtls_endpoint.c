
#include "tinylib/ssl/dtls_endpoint.h"
#include "tinylib/util/log.h"

#ifdef WIN32
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <stdint.h>

#include <openssl/ssl.h>
#include <openssl/opensslv.h>

loop_t *loop;

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

static
void on_handshanke(dtls_endoint_t* dtls_endoint, int ok, void* userdata)
{
    static char test_dtls_rtp_msg[32];
    rtp_head_t *rtp = (rtp_head_t*)test_dtls_rtp_msg;
	rtp->V = 2;
	rtp->P = 0;
	rtp->X = 0;
	rtp->CC = 0;
	rtp->PT = 96;
	rtp->ssrc = 1234567;
    rtp->M = 0;
    rtp->SN = 0;
    rtp->timestamp = 0;
    strncpy((char*)&rtp[1], "test_dtls_srtp_msg", 19);

    if (ok)
    {
        printf("dtls handshake ok\n");
        dtls_endoint_send(dtls_endoint, test_dtls_rtp_msg, sizeof(test_dtls_rtp_msg));
    }
    else
    {
        printf("dtls handshake failed\n");
        loop_quit(loop);
    }

    return;
}

void on_shutdown(dtls_endoint_t* dtls_endoint, int normal, void* userdata)
{
    printf("dtls connection is down\n");
    loop_quit(loop);
    return;
}

void on_message(dtls_endoint_t* dtls_endoint, void *message, unsigned size, void* userdata)
{
    rtp_head_t *rtp = (rtp_head_t*)message;

    printf("dtls message:\n"
           "  rtp, V:%d, P:%d, X:%d, CC:%d, M:%d, PT:%d, SN:%u, timestamp: %u, ssrc: %u\n  ", 
        rtp->V, rtp->P, rtp->X, rtp->CC, rtp->M, rtp->PT, rtp->SN, rtp->timestamp, rtp->ssrc);
    fwrite(&rtp[1], 1, (size - sizeof(*rtp)), stdout);
    printf("\n");

    return;
}

static
void on_interrupt(int signo)
{
    loop_quit(loop);
    return;
}

int main(int argc, char *argv[])
{
    dtls_endoint_t* dtls_endoint;
    
    const char *peer_ip = "192.168.137.2";
    unsigned short peer_port = 7443;
    unsigned short local_port = 7443;

    const char *ca_file;
    const char *key_file;
    const char *ca_pwd;
    
    enum dtls_endpoint_mode endpoint_mode = DTLS_ENDPOINT_MODE_CLIENT;
    
    if (argc < 8)
    {
        printf("usage: %s <local port> <peer ip> <peer port> <ca file> <ca private key file> <ca password> <mode, c/s>\n", argv[0]);
        return 0;
    }
    local_port = (unsigned short)atoi(argv[1]);
    peer_ip = argv[2];
    peer_port = (unsigned short)atoi(argv[3]);
    ca_file = argv[4];
    key_file = argv[5];
    ca_pwd = argv[6];
    endpoint_mode = (argv[7][0] == 's') ? DTLS_ENDPOINT_MODE_SERVER : DTLS_ENDPOINT_MODE_CLIENT;

  #ifdef WIN32
    {
        WSADATA wsadata;
        WSAStartup(MAKEWORD(2, 2), &wsadata);
    }
  #endif

    printf("openssl version: %s\n", OPENSSL_VERSION_TEXT);

    SSL_library_init();
    SSL_load_error_strings();

    loop = loop_new(64);
    dtls_endoint = dtls_endoint_new(loop, 
        peer_ip, peer_port, NULL, local_port,
        on_handshanke, on_shutdown, on_message, NULL, NULL,
        ca_file, key_file, ca_pwd, endpoint_mode);
  #if 1
    dtls_endoint_enable_srtp(dtls_endoint);
  #endif
    dtls_endoint_start(dtls_endoint);
    signal(SIGINT, on_interrupt);
    loop_loop(loop);
    
    printf("== quit ==\n");

    dtls_endoint_destroy(dtls_endoint);
    loop_destroy(loop);

  #ifdef WIN32
    WSACleanup();
  #endif

    return 0;
}
