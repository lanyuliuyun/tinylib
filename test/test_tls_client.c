
#include "tinylib/ssl/tls_client.h"
#include "tinylib/util/log.h"

#ifdef WIN32
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <openssl/ssl.h>

loop_t *loop;

static
void on_connect(tls_client_t* tls_client, int ok, void* userdata)
{
    const char *http_request = 
        "GET / HTTP/1.1\r\n"
        "Host: 192.168.137.2:8443\r\n"
        "User-Agent: curl/7.47.0\r\n"
        "Connection: close\r\n"
        "Accept: text/plain\r\n"
        "\r\n";
    
    log_info("on_connect: tls_client: %p, ok: %d", tls_client, ok);
    
    tls_client_send(tls_client, http_request, strlen(http_request));
    
    return;
}

static
void on_data(tls_client_t* tls_client, buffer_t* buffer, void* userdata)
{
    void *data = buffer_peek(buffer);
    unsigned size = buffer_readablebytes(buffer);
    
    fwrite(data, 1, size, stdout);
    fwrite("\n\n", 1, 2, stdout);

    return;
}

static
void on_close(tls_client_t* tls_client, void* userdata)
{
    log_info("on_close: tls_client: %p", tls_client);
    tls_client_destroy(tls_client);
    loop_quit(loop);
    
    return;
}

int main(int argc, char *argv[])
{
    tls_client_t* tls_client;
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

    /* log_setlevel(LOG_LEVEL_DEBUG); */

    loop = loop_new(64);
    tls_client = tls_client_new(loop, 
        "192.168.137.2", 8443, 
        on_connect, on_data, on_close, NULL, 
        ca_file, key_file, ca_pwd);
    tls_client_connect(tls_client);
    loop_loop(loop);

    /* tls_client_destroy(tls_client); */
    loop_destroy(loop);

  #ifdef WIN32
    WSACleanup();
  #endif

    return 0;
}
