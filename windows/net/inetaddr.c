
#include "net/inetaddr.h"

#include "util/log.h"
#include <string.h>

#include <winsock2.h>

void inetaddr_init(inetaddr_t *addr, struct sockaddr_in *addr_in)
{
    if (NULL == addr || NULL == addr_in)
    {
        log_error("inetaddr_init: bad addr(%p) or bad addr_in(%p)", addr, addr_in);
        return;
    }

    memset(addr, 0, sizeof(*addr));
    strncpy(addr->ip, inet_ntoa(addr_in->sin_addr), 15);
    addr->port = ntohs(addr_in->sin_port);
}

void inetaddr_initbyipport(inetaddr_t *addr, const char *ip, unsigned short port)
{
    if (NULL == addr)
    {
        log_error("inetaddr_initbyipport: bad addr(%p) ", addr);
        return;
    }

    memset(addr, 0, sizeof(*addr));
    if (NULL != ip)
    {
        strncpy(addr->ip, ip, 15);
    }
    else
    {
        strncpy(addr->ip, "0.0.0.0", 15);   /* INADDR_ANY */
    }
    addr->port = port;

    return;
}
