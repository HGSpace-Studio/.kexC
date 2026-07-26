#ifndef KAPI_NETDEVICE_H
#define KAPI_NETDEVICE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct kapi_netdev kapi_netdev_t;

#define KAPI_NETDEV_NAME_LEN 16
#define KAPI_ETH_ALEN 6

#define KAPI_NETDEV_UP         (1 << 0)
#define KAPI_NETDEV_BROADCAST  (1 << 1)
#define KAPI_NETDEV_MULTICAST  (1 << 2)
#define KAPI_NETDEV_LOOPBACK   (1 << 3)
#define KAPI_NETDEV_POINTTOPOINT (1 << 4)
#define KAPI_NETDEV_PROMISC    (1 << 5)
#define KAPI_NETDEV_ALLMULTI   (1 << 6)
#define KAPI_NETDEV_NOARP      (1 << 7)

typedef struct {
    uint64_t rx_packets;
    uint64_t tx_packets;
    uint64_t rx_bytes;
    uint64_t tx_bytes;
    uint64_t rx_errors;
    uint64_t tx_errors;
    uint64_t rx_dropped;
    uint64_t tx_dropped;
    uint64_t multicast;
    uint64_t collisions;
} kapi_netdev_stats_t;

kapi_netdev_t* kapi_netdev_open(const char* name);

int kapi_netdev_close(kapi_netdev_t* dev);

int kapi_netdev_up(kapi_netdev_t* dev);

int kapi_netdev_down(kapi_netdev_t* dev);

int kapi_netdev_get_mac(kapi_netdev_t* dev, uint8_t mac[KAPI_ETH_ALEN]);

int kapi_netdev_set_mac(kapi_netdev_t* dev, const uint8_t mac[KAPI_ETH_ALEN]);

int kapi_netdev_get_mtu(kapi_netdev_t* dev);

int kapi_netdev_set_mtu(kapi_netdev_t* dev, int mtu);

uint32_t kapi_netdev_get_flags(kapi_netdev_t* dev);

int kapi_netdev_set_flags(kapi_netdev_t* dev, uint32_t flags);

int kapi_netdev_set_promiscuous(kapi_netdev_t* dev, int enable);

int kapi_netdev_is_up(kapi_netdev_t* dev);

int kapi_netdev_get_stats(kapi_netdev_t* dev, kapi_netdev_stats_t* stats);

int kapi_netdev_send(kapi_netdev_t* dev, const void* data, size_t len);

int kapi_netdev_count(void);

int kapi_netdev_get_name(kapi_netdev_t* dev, char* name, size_t len);

#ifdef __cplusplus
}
#endif

#endif