
#include "tinylib/ssl/tls_client.h"
#include "tinylib/util/log.h"

#ifdef WIN32
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>

#include <openssl/ssl.h>

loop_t *loop;
loop_timer_t *timer = NULL;
FILE *in_file = NULL;
FILE *out_file = NULL;

static
int random_packet[16384];

static
void on_expire(void *userdata)
{
    loop_quit(loop);
    return;
}

static
void on_interval(void *userdata)
{
  #if 1
    tls_client_t *tls_client = (tls_client_t*)userdata;
    size_t len = fread(random_packet, 1, 10240, in_file);
    if (len > 0)
    {
        tls_client_send(tls_client, random_packet, len);
    }
    else
    {
        /* 稍微延迟一段时间，保证收完所有的数据 */
        (void*)loop_runafter(loop, 10000, on_expire, loop);
    }
  #else
    tls_client_send(tls_client, random_packet, sizeof(random_packet));
  #endif

    return;
}

static
void on_connect(tls_client_t* tls_client, int ok, void* userdata)
{
    log_info("on_connect: tls_client: %p", tls_client);
    if (ok)
    {
        timer = loop_runevery(loop, 10, on_interval, tls_client);
    }
    else
    {
        loop_quit(loop);
    }
    return;
}

static
void on_data(tls_client_t* tls_client, buffer_t* buffer, void* userdata)
{
    
  #if 1    
    size_t len = fwrite(buffer_peek(buffer), 1, buffer_readablebytes(buffer), out_file);
    buffer_retrieve(buffer, len);
  #else
    buffer_retrieveall(buffer);
  #endif

    return;
}

static
void on_close(tls_client_t* tls_client, void* userdata)
{
    log_info("on_close: tls_client: %p", tls_client);
    loop_quit(loop);
    
    return;
}

void fill_random_packet(void)
{
    int i;
    for (i = 0; i < sizeof(random_packet)/sizeof(random_packet[0]); i++)
    {
        random_packet[i] = (int)random();
    }

    return;
}

int main(int argc, char *argv[])
{
    tls_client_t* tls_client;

    if (argc < 5)
    {
        printf("usage: %s <ssl echo server ip> <ssl echo server port> <input file> <out file>\n", argv[0]);
        return 0;
    }

  #ifdef WIN32
    {
        WSADATA wsadata;
        WSAStartup(MAKEWORD(2, 2), &wsadata);
    }
  #endif

    SSL_library_init();
    SSL_load_error_strings();

    /* log_setlevel(LOG_LEVEL_DEBUG); */
    
    in_file = fopen(argv[3], "rb");
    out_file = fopen(argv[4], "wb");

    loop = loop_new(64);
    tls_client = tls_client_new(loop, argv[1], (unsigned short)atoi(argv[2]), on_connect, on_data, on_close, NULL);
  #if 0
    if (argc > 5)
    {
        tls_client_use_ca(tls_client, argv[3], argv[4], argv[5]);
    }
  #endif
    tls_client_connect(tls_client);
    loop_loop(loop);

    /* tls_client_destroy(tls_client); */
    loop_cancel(loop, timer);
    loop_destroy(loop);

    fclose(in_file);
    fclose(out_file);

  #ifdef WIN32
    WSACleanup();
  #endif

    return 0;
}
