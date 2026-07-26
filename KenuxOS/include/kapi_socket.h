#ifndef KAPI_SOCKET_H
#define KAPI_SOCKET_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int kapi_sockfd_t;

#define KAPI_AF_UNIX         1
#define KAPI_AF_INET         2

#define KAPI_SOCK_STREAM     1
#define KAPI_SOCK_DGRAM      2
#define KAPI_SOCK_RAW        3

#define KAPI_IPPROTO_TCP     6
#define KAPI_IPPROTO_UDP     17

#define KAPI_SOL_SOCKET      1

#define KAPI_SO_REUSEADDR     2
#define KAPI_SO_KEEPALIVE     9
#define KAPI_SO_BROADCAST     6
#define KAPI_SO_RCVBUF        8
#define KAPI_SO_SNDBUF        7
#define KAPI_SO_RCVTIMEO      20
#define KAPI_SO_SNDTIMEO      21
#define KAPI_SO_ERROR         4

#define KAPI_SHUT_RD           0
#define KAPI_SHUT_WR           1
#define KAPI_SHUT_RDWR         2

#define KAPI_MSG_PEEK          0x02
#define KAPI_MSG_DONTWAIT      0x40
#define KAPI_MSG_WAITALL       0x100

typedef struct {
    uint16_t sin_family;
    uint16_t sin_port;
    uint32_t sin_addr;
    uint8_t  sin_zero[8];
} kapi_sockaddr_in_t;

typedef struct {
    uint16_t sun_family;
    char     sun_path[108];
} kapi_sockaddr_un_t;

typedef union {
    kapi_sockaddr_in_t in;
    kapi_sockaddr_un_t un;
} kapi_sockaddr_t;

kapi_sockfd_t kapi_socket(int domain, int type, int protocol);

int kapi_bind(kapi_sockfd_t sockfd, const kapi_sockaddr_t* addr, size_t addrlen);

int kapi_listen(kapi_sockfd_t sockfd, int backlog);

kapi_sockfd_t kapi_accept(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen);

int kapi_connect(kapi_sockfd_t sockfd, const kapi_sockaddr_t* addr, size_t addrlen);

int64_t kapi_send(kapi_sockfd_t sockfd, const void* buf, size_t len, int flags);

int64_t kapi_recv(kapi_sockfd_t sockfd, void* buf, size_t len, int flags);

int64_t kapi_sendto(kapi_sockfd_t sockfd, const void* buf, size_t len, int flags,
                    const kapi_sockaddr_t* dest_addr, size_t addrlen);

int64_t kapi_recvfrom(kapi_sockfd_t sockfd, void* buf, size_t len, int flags,
                      kapi_sockaddr_t* src_addr, size_t* addrlen);

int kapi_setsockopt(kapi_sockfd_t sockfd, int level, int optname,
                    const void* optval, size_t optlen);

int kapi_getsockopt(kapi_sockfd_t sockfd, int level, int optname,
                    void* optval, size_t* optlen);

int kapi_shutdown(kapi_sockfd_t sockfd, int how);

int kapi_close_socket(kapi_sockfd_t sockfd);

int kapi_getsockname(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen);

int kapi_getpeername(kapi_sockfd_t sockfd, kapi_sockaddr_t* addr, size_t* addrlen);

#ifdef __cplusplus
}
#endif

#endif