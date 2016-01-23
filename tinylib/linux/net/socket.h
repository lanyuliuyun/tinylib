
#ifndef NET_SOCKET_H
#define NET_SOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

int create_server_socket(unsigned short port, const char* ip);

int create_client_socket(void);

void set_socket_reuseaddr(int fd, int on);

void set_socket_onblock(int fd, int on);

void set_socket_nodelay(int fd, int on);

int create_udp_socket(unsigned short port, const char *ip);

#ifdef __cplusplus
}
#endif

#endif /* !NET_SOCKET_H */

