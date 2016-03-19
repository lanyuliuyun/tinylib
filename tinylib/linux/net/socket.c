
#include "tinylib/linux/net/socket.h"
#include "tinylib/util/log.h"

#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

int create_server_socket(unsigned short port, const char* ip)
{
    int fd;
    struct sockaddr_in addr;
    int result;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
    {
        log_error("create_server_socket: socket() failed, errno: %d, addr: %s:%u", errno, ip, port);    
        return -1;
    }

    set_socket_onblock(fd, 1);
    set_socket_reuseaddr(fd, 1);

    memset(&addr, 0, sizeof(addr));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = (NULL != ip ? inet_addr(ip) : INADDR_ANY);
    addr.sin_port = htons(port);
    result = bind(fd, (struct sockaddr*)&addr, sizeof(addr));
    if (result < 0)
    {
        log_error("create_server_socket: bind() failed, erron: %d, addr: %s:%u", errno, ip, port);
        close(fd);
        return -1;
    }

    return fd;
}

int create_client_socket(void)
{
    int fd;

    fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0)
    {
        log_error("create_client_socket: socket() failed, errno: %d", errno);
        return -1;
    }

    set_socket_onblock(fd, 1);

    return fd;
}

void set_socket_reuseaddr(int fd, int on)
{
    int value = on ? 1 :0;
    if (fd < 0)
    {
        log_error("set_socket_reuseaddr: bad fd");
        return;
    }

    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(value)) < 0)
    {
        log_error("set_socket_reuseaddr: setsockopt() failed, errno: %d", errno);
    }

    return;
}

void set_socket_onblock(int fd, int on)
{
    int flags;
    int result;

    if (fd < 0)
    {
        log_error("set_socket_onblock: bad fd");
        return;
    }

    flags = fcntl(fd, F_GETFL, 0);
    if (on)
    {
        flags |= O_NONBLOCK;
    }
    else
    {
        flags &= ~O_NONBLOCK;
    }
    result = fcntl(fd, F_SETFL, flags);

    flags = fcntl(fd, F_GETFD, 0);
    flags |= FD_CLOEXEC;
    result = fcntl(fd, F_SETFD, flags);
    
    (void)result;

    return;
}

void set_socket_nodelay(int fd, int on)
{
    int value = on ? 1 :0;
    int result;
    
    if (fd < 0)
    {
        log_error("set_socket_nodelay: bad fd");
        return;
    }

    result = setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &value, sizeof(value));
    
    (void)result;

    return;
}

int create_udp_socket(unsigned short port, const char *ip)
{
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0)
    {
        log_error("create_udp_socket: socket() failed, errno: %d, addr: %s:%u", errno, ip, port);
        return -1;
    }

    set_socket_onblock(fd, 1);

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = (NULL != ip ? inet_addr(ip) : INADDR_ANY);
    addr.sin_port = htons(port);
    if (bind(fd, (struct sockaddr*)&addr, sizeof(addr)) != 0)
    {
        log_error("create_udp_socket: bind() failed, erron: %d, addr: %s:%u", errno, ip, port);
        close(fd);
        return -1;
    }

    return fd;
}
