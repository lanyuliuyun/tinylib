
#include "tinylib/ssl/dtls_client.h"
#include "tinylib/util/log.h"

#ifdef WIN32
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <openssl/ssl.h>

static
void on_handshanke(dtls_client_t* dtls_client, int ok, void* userdata)
{
    return;
}

void on_shutdown(dtls_client_t* dtls_client, int normal, void* userdata)
{
    return;
}

void on_message(dtls_client_t* dtls_client, void *message, unsigned size, void* userdata)
{
    return;
}

int main(int argc, char *argv[])
{
    loop_t *loop;
    dtls_client_t* dtls_client;
    const char *ca_file;
    const char *key_file;
    const char *ca_pwd;
    
    if (argc < 4)
    {
        printf("usage: %s <ca file> <ca private key file> <ca password>\n", argv[0]);
        return 0;
    }
    ca_file = argv[1];
    key_file = argv[2];
    ca_pwd = argv[3];

  #ifdef WIN32
    {
        WSADATA wsadata;
        WSAStartup(MAKEWORD(2, 2), &wsadata);
    }
  #endif
  
    SSL_library_init();
    SSL_load_error_strings();

    loop = loop_new(64);
    dtls_client = dtls_client_new(loop, 
        "192.168.137.2", 4443, NULL, 4443,
        on_handshanke, on_shutdown, on_message, NULL, NULL,
        ca_file, key_file, ca_pwd);
    dtls_client_start(dtls_client);
    loop_loop(loop);

    dtls_client_destroy(dtls_client);
    loop_destroy(loop);

  #ifdef WIN32
    WSACleanup();
  #endif

    return 0;
}
