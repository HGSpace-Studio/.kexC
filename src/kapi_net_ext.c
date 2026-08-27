#include "kapi_net_ext.h"
#include "kapi.h"
#include <string.h>

static kapi_netif_info_t netif_table[32];
static int netif_count = 0;

static kapi_packet_handler_t pkt_handlers[128];
static void* pkt_handler_data[128];
static kapi_connection_handler_t conn_handlers[128];
static void* conn_handler_data[128];
static kapi_error_handler_t err_handlers[128];
static void* err_handler_data[128];

int kapi_net_ext_init(void)
{
    memset(netif_table, 0, sizeof(netif_table));
    netif_count = 0;
    memset(pkt_handlers, 0, sizeof(pkt_handlers));
    memset(conn_handlers, 0, sizeof(conn_handlers));
    memset(err_handlers, 0, sizeof(err_handlers));
    return KAPI_OK;
}

int kapi_socketpair(int domain, int type, int protocol, int sv[2])
{
    if (!sv) return KAPI_EINVAL;
    sv[0] = kapi_socket(domain, type, protocol);
    sv[1] = kapi_socket(domain, type, protocol);
    if (sv[0] < 0 || sv[1] < 0) return KAPI_ERROR;
    return KAPI_OK;
}

int kapi_accept4(int sockfd, struct sockaddr* addr, socklen_t* addrlen, int flags)
{
    int fd = kapi_accept(sockfd, addr, addrlen);
    if (fd < 0) return fd;
    if (flags & KAPI_SOCK_NONBLOCK) {
        kapi_set_nonblocking(fd, true);
    }
    return fd;
}

ssize_t kapi_sendmsg(int sockfd, const struct msghdr* msg, int flags)
{
    if (!msg) return KAPI_EINVAL;
    if (msg->msg_iovlen == 0) return 0;
    ssize_t total = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        ssize_t n = kapi_send(sockfd, msg->msg_iov[i].iov_base,
                              msg->msg_iov[i].iov_len, flags);
        if (n < 0) return n;
        total += n;
    }
    return total;
}

ssize_t kapi_recvmsg(int sockfd, struct msghdr* msg, int flags)
{
    if (!msg) return KAPI_EINVAL;
    if (msg->msg_iovlen == 0) return 0;
    ssize_t total = 0;
    for (size_t i = 0; i < msg->msg_iovlen; i++) {
        ssize_t n = kapi_recv(sockfd, msg->msg_iov[i].iov_base,
                              msg->msg_iov[i].iov_len, flags);
        if (n < 0) return n;
        total += n;
    }
    return total;
}

uint16_t kapi_htons(uint16_t hostshort)
{
    return ((hostshort & 0xFF) << 8) | ((hostshort >> 8) & 0xFF);
}

uint32_t kapi_htonl(uint32_t hostlong)
{
    return ((hostlong & 0xFF) << 24) | ((hostlong & 0xFF00) << 8) |
           ((hostlong >> 8) & 0xFF00) | ((hostlong >> 24) & 0xFF);
}

uint16_t kapi_ntohs(uint16_t netshort) { return kapi_htons(netshort); }
uint32_t kapi_ntohl(uint32_t netlong) { return kapi_htonl(netlong); }

int kapi_inet_pton(int af, const char* src, void* dst)
{
    if (!src || !dst) return KAPI_EINVAL;
    if (af == KAPI_AF_INET) {
        uint32_t addr = 0;
        int shift = 24;
        const char* p = src;
        while (*p && shift >= 0) {
            uint32_t octet = 0;
            while (*p >= '0' && *p <= '9') {
                octet = octet * 10 + (uint32_t)(*p - '0');
                p++;
            }
            addr |= (octet << shift);
            shift -= 8;
            if (*p == '.') p++;
        }
        *(uint32_t*)dst = kapi_htonl(addr);
        return 1;
    }
    return KAPI_ENOSYS;
}

const char* kapi_inet_ntop(int af, const void* src, char* dst, socklen_t size)
{
    if (!src || !dst) return NULL;
    if (af == KAPI_AF_INET) {
        uint32_t addr = kapi_ntohl(*(const uint32_t*)src);
        snprintf(dst, (size_t)size, "%u.%u.%u.%u",
                 (addr >> 24) & 0xFF, (addr >> 16) & 0xFF,
                 (addr >> 8) & 0xFF, addr & 0xFF);
        return dst;
    }
    return NULL;
}

int kapi_get_socket_info(int sockfd, kapi_socket_info_t* info)
{
    if (!info) return KAPI_EINVAL;
    memset(info, 0, sizeof(*info));
    info->domain = KAPI_AF_INET;
    info->send_buf_size = 8192;
    info->recv_buf_size = 8192;
    (void)sockfd;
    return KAPI_OK;
}

int kapi_set_nonblocking(int sockfd, bool nonblocking)
{
    (void)sockfd; (void)nonblocking;
    return KAPI_OK;
}

int kapi_set_reuseaddr(int sockfd, bool reuse)
{
    int val = reuse ? 1 : 0;
    return kapi_setsockopt(sockfd, KAPI_SOL_SOCKET, KAPI_SO_REUSEADDR, &val, sizeof(val));
}

int kapi_set_keepalive(int sockfd, bool keepalive, int idle, int interval, int count)
{
    int val = keepalive ? 1 : 0;
    int ret = kapi_setsockopt(sockfd, KAPI_SOL_SOCKET, KAPI_SO_KEEPALIVE, &val, sizeof(val));
    (void)idle; (void)interval; (void)count;
    return ret;
}

int kapi_set_broadcast(int sockfd, bool broadcast)
{
    int val = broadcast ? 1 : 0;
    return kapi_setsockopt(sockfd, KAPI_SOL_SOCKET, KAPI_SO_BROADCAST, &val, sizeof(val));
}

int kapi_set_linger(int sockfd, bool enabled, int seconds)
{
    struct kapi_linger l;
    l.l_onoff = enabled ? 1 : 0;
    l.l_linger = seconds;
    return kapi_setsockopt(sockfd, KAPI_SOL_SOCKET, KAPI_SO_LINGER, &l, sizeof(l));
}

int kapi_set_sndbuf(int sockfd, int size)
{
    return kapi_setsockopt(sockfd, KAPI_SOL_SOCKET, KAPI_SO_SNDBUF, &size, sizeof(size));
}

int kapi_set_rcvbuf(int sockfd, int size)
{
    return kapi_setsockopt(sockfd, KAPI_SOL_SOCKET, KAPI_SO_RCVBUF, &size, sizeof(size));
}

int kapi_set_timeout(int sockfd, int send_timeout, int recv_timeout)
{
    (void)sockfd; (void)send_timeout; (void)recv_timeout;
    return KAPI_OK;
}

int kapi_tcp_nodelay(int sockfd, bool nodelay)
{
    int val = nodelay ? 1 : 0;
    return kapi_setsockopt(sockfd, 6, 1, &val, sizeof(val));
}

int kapi_tcp_cork(int sockfd, bool cork)
{
    int val = cork ? 1 : 0;
    return kapi_setsockopt(sockfd, 6, 3, &val, sizeof(val));
}

int kapi_tcp_keepidle(int sockfd, int seconds)
{
    return kapi_setsockopt(sockfd, 6, 4, &seconds, sizeof(seconds));
}

int kapi_tcp_keepintvl(int sockfd, int seconds)
{
    return kapi_setsockopt(sockfd, 6, 5, &seconds, sizeof(seconds));
}

int kapi_tcp_keepcnt(int sockfd, int probes)
{
    return kapi_setsockopt(sockfd, 6, 6, &probes, sizeof(probes));
}

int kapi_tcp_quickack(int sockfd, bool quickack)
{
    int val = quickack ? 1 : 0;
    return kapi_setsockopt(sockfd, 6, 12, &val, sizeof(val));
}

int kapi_tcp_congestion(int sockfd, const char* name)
{
    (void)sockfd; (void)name;
    return KAPI_OK;
}

int kapi_tcp_window_clamp(int sockfd, int window)
{
    return kapi_setsockopt(sockfd, 6, 10, &window, sizeof(window));
}

int kapi_udp_connect(int sockfd, const struct sockaddr* addr, socklen_t addrlen)
{
    return kapi_connect(sockfd, addr, addrlen);
}

int kapi_udp_disconnect(int sockfd)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = KAPI_AF_INET;
    return kapi_connect(sockfd, (struct sockaddr*)&addr, sizeof(addr));
}

int kapi_get_ifcount(void) { return netif_count; }

int kapi_get_iflist(kapi_netif_info_t* ifs, int count)
{
    if (!ifs) return KAPI_EINVAL;
    int n = count < netif_count ? count : netif_count;
    memcpy(ifs, netif_table, (size_t)n * sizeof(kapi_netif_info_t));
    return n;
}

int kapi_get_ifinfo(const char* name, kapi_netif_info_t* info)
{
    if (!name || !info) return KAPI_EINVAL;
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            *info = netif_table[i];
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_ifup(const char* name)
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            netif_table[i].is_up = true;
            netif_table[i].flags |= 1;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_ifdown(const char* name)
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            netif_table[i].is_up = false;
            netif_table[i].flags &= ~1;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_ifaddr(const char* name, uint32_t addr)
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            netif_table[i].ipv4_addr = addr;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_ifnetmask(const char* name, uint32_t mask)
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            netif_table[i].netmask = mask;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_ifbroadcast(const char* name, uint32_t addr)
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            netif_table[i].broadcast = addr;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_ifmtu(const char* name, int mtu)
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            netif_table[i].mtu = mtu;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_ifflags(const char* name, int flags)
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            netif_table[i].flags = flags;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_ifmac(const char* name, const uint8_t mac[6])
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            memcpy(netif_table[i].mac_addr, mac, 6);
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_set_promisc(const char* name, bool enable)
{
    for (int i = 0; i < netif_count; i++) {
        if (strcmp(netif_table[i].name, name) == 0) {
            netif_table[i].is_promisc = enable;
            return KAPI_OK;
        }
    }
    return KAPI_ENOENT;
}

int kapi_register_packet_handler(int sockfd, kapi_packet_handler_t handler, void* data)
{
    if (sockfd < 0 || sockfd >= 128) return KAPI_EINVAL;
    pkt_handlers[sockfd] = handler;
    pkt_handler_data[sockfd] = data;
    return KAPI_OK;
}

int kapi_unregister_packet_handler(int sockfd)
{
    if (sockfd < 0 || sockfd >= 128) return KAPI_EINVAL;
    pkt_handlers[sockfd] = NULL;
    pkt_handler_data[sockfd] = NULL;
    return KAPI_OK;
}

int kapi_register_connection_handler(int server_fd, kapi_connection_handler_t handler, void* data)
{
    if (server_fd < 0 || server_fd >= 128) return KAPI_EINVAL;
    conn_handlers[server_fd] = handler;
    conn_handler_data[server_fd] = data;
    return KAPI_OK;
}

int kapi_unregister_connection_handler(int server_fd)
{
    if (server_fd < 0 || server_fd >= 128) return KAPI_EINVAL;
    conn_handlers[server_fd] = NULL;
    conn_handler_data[server_fd] = NULL;
    return KAPI_OK;
}

int kapi_register_error_handler(int sockfd, kapi_error_handler_t handler, void* data)
{
    if (sockfd < 0 || sockfd >= 128) return KAPI_EINVAL;
    err_handlers[sockfd] = handler;
    err_handler_data[sockfd] = data;
    return KAPI_OK;
}

int kapi_unregister_error_handler(int sockfd)
{
    if (sockfd < 0 || sockfd >= 128) return KAPI_EINVAL;
    err_handlers[sockfd] = NULL;
    err_handler_data[sockfd] = NULL;
    return KAPI_OK;
}