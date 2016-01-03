
#ifndef NET_INET_ADDR_H
#define NET_INET_ADDR_H

#ifdef __cplusplus
extern "C" {
#endif

struct sockaddr_in;
typedef struct inetaddr
{
    char ip[16];  /* 255.255.255.255 */
    unsigned short port;
}inetaddr_t;

void inetaddr_init(inetaddr_t *addr, struct sockaddr_in *addr_in);

void inetaddr_initbyipport(inetaddr_t *addr, const char *ip, unsigned short port);

#ifdef __cplusplus
}
#endif

#endif /* !NET_INET_ADDR_H  */

