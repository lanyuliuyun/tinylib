
#include "tinylib/ssl/dtls_endpoint.h"
#include "tinylib/util/log.h"

#ifdef WIN32
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#include <openssl/ssl.h>
#include <openssl/opensslv.h>

loop_t *loop;

static
void on_handshanke(dtls_endoint_t* dtls_endoint, int ok, void* userdata)
{
    static const char *msg = "test dtls message\n";
    
    if (ok)
    {
        printf("dtls handshake ok\n");
        dtls_endoint_send(dtls_endoint, msg, strlen(msg));
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
    printf("dtls message: ");
    fwrite(message, 1, size, stdout);
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
  #if 0
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
