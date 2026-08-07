#include "kex_user.h"

struct kex_socket_addr {
    unsigned short sa_family;
    char sa_data[14];
};

struct kex_sockaddr_in {
    unsigned short sin_family;
    unsigned short sin_port;
    unsigned int sin_addr;
    char sin_zero[8];
};

static void print_str(const char *s) {
    sys_write(1, s, kex_strlen(s));
}

static void print_kv(const char *key, long val) {
    print_str(key);
    kex_printlong(val, 1);
}

static void print_ip(unsigned int addr) {
    unsigned char *p = (unsigned char *)&addr;
    char buf[16];
    int pos = 0;
    for (int i = 0; i < 4; i++) {
        unsigned char b = p[i];
        if (b == 0) {
            buf[pos++] = '0';
        } else {
            char tmp[4];
            int t = 0;
            while (b > 0) { tmp[t++] = '0' + (b % 10); b /= 10; }
            while (t > 0) buf[pos++] = tmp[--t];
        }
        if (i < 3) buf[pos++] = '.';
    }
    buf[pos] = '\0';
    print_str(buf);
}

void _start(void) {
    kex_puts("========================================");
    kex_puts("   Kenux Network Status (netstat)      ");
    kex_puts("========================================");
    kex_puts("");

    kex_puts("--- Socket Creation Test ---");
    long tcp_sock = sys_socket(2, 1, 0);
    print_kv("  TCP socket (AF_INET, SOCK_STREAM): ", tcp_sock);
    if (tcp_sock >= 0) sys_close((int)tcp_sock);

    long udp_sock = sys_socket(2, 2, 0);
    print_kv("  UDP socket (AF_INET, SOCK_DGRAM):  ", udp_sock);
    if (udp_sock >= 0) sys_close((int)udp_sock);

    long raw_sock = sys_socket(2, 3, 0);
    print_kv("  RAW socket (AF_INET, SOCK_RAW):    ", raw_sock);
    if (raw_sock >= 0) sys_close((int)raw_sock);

    long unix_sock = sys_socket(1, 1, 0);
    print_kv("  UNIX socket (AF_UNIX, SOCK_STREAM): ", unix_sock);
    if (unix_sock >= 0) sys_close((int)unix_sock);
    kex_puts("");

    kex_puts("--- UDP Echo Test ---");
    long sock = sys_socket(2, 2, 0);
    if (sock >= 0) {
        struct kex_sockaddr_in bind_addr;
        kex_memset(&bind_addr, 0, sizeof(bind_addr));
        bind_addr.sin_family = 2;
        bind_addr.sin_port = ((8080 & 0xFF) << 8) | ((8080 >> 8) & 0xFF);
        bind_addr.sin_addr = 0;

        long br = sys_bind((int)sock, &bind_addr, sizeof(bind_addr));
        print_kv("  bind to 0.0.0.0:8080: ", br);

        const char *msg = "KenuxOS UDP test packet";
        long slen = kex_strlen(msg);

        struct kex_sockaddr_in dest;
        kex_memset(&dest, 0, sizeof(dest));
        dest.sin_family = 2;
        dest.sin_port = ((9090 & 0xFF) << 8) | ((9090 >> 8) & 0xFF);
        dest.sin_addr = (127 << 0) | (0 << 8) | (0 << 16) | (1 << 24);

        long sent = sys_sendto((int)sock, msg, slen, 0, &dest, sizeof(dest));
        print_kv("  sendto localhost:9090: ", sent);
        if (sent > 0) {
            print_str("  Sent ");
            kex_printlong(sent, 0);
            print_str(" bytes: \"");
            print_str(msg);
            print_str("\"\n");
        }

        char recvbuf[256];
        kex_memset(recvbuf, 0, sizeof(recvbuf));
        struct kex_sockaddr_in from;
        kex_memset(&from, 0, sizeof(from));
        int fromlen = sizeof(from);

        long recvd = sys_recvfrom((int)sock, recvbuf, sizeof(recvbuf) - 1, 0,
                                   &from, &fromlen);
        if (recvd > 0) {
            recvbuf[recvd] = '\0';
            print_str("  Received ");
            kex_printlong(recvd, 0);
            print_str(" bytes from ");
            print_ip(from.sin_addr);
            kex_putchar('\n');
            print_str("  Data: \"");
            print_str(recvbuf);
            print_str("\"\n");
        } else {
            print_kv("  recvfrom result (timeout/no data): ", recvd);
        }

        sys_close((int)sock);
    } else {
        print_kv("  socket creation failed: ", sock);
    }
    kex_puts("");

    kex_puts("--- Pipe Test ---");
    int pipefd[2];
    kex_memset(pipefd, 0, sizeof(pipefd));
    long pret = sys_pipe(pipefd);
    print_kv("  pipe(): ", pret);
    if (pret == 0) {
        const char *pmsg = "Hello through pipe!";
        long wret = sys_write(pipefd[1], pmsg, kex_strlen(pmsg));
        print_kv("  write to pipe: ", wret);

        char pbuf[64];
        kex_memset(pbuf, 0, sizeof(pbuf));
        long rret = sys_read(pipefd[0], pbuf, sizeof(pbuf) - 1);
        if (rret > 0) {
            pbuf[rret] = '\0';
            print_str("  read from pipe: \"");
            print_str(pbuf);
            print_str("\"\n");
        }
        sys_close(pipefd[0]);
        sys_close(pipefd[1]);
    }
    kex_puts("");

    kex_puts("--- Socketpair Test ---");
    int sv[2];
    kex_memset(sv, 0, sizeof(sv));
    long spr = sys_socketpair(1, 1, 0, sv);
    print_kv("  socketpair(): ", spr);
    if (spr == 0) {
        const char *smsg = "socketpair test";
        sys_write(sv[0], smsg, kex_strlen(smsg));
        char sbuf[64];
        kex_memset(sbuf, 0, sizeof(sbuf));
        long sr = sys_read(sv[1], sbuf, sizeof(sbuf) - 1);
        if (sr > 0) {
            sbuf[sr] = '\0';
            print_str("  Received via socketpair: \"");
            print_str(sbuf);
            print_str("\"\n");
        }
        sys_close(sv[0]);
        sys_close(sv[1]);
    }
    kex_puts("");

    kex_puts("--- Network Device Info ---");
    long nfd = kenux_net_attach("eth0", 0);
    print_kv("  net_attach(eth0): ", nfd);
    if (nfd >= 0) {
        char info[256];
        kex_memset(info, 0, sizeof(info));
        long ioret = kenux_net_ioctl((int)nfd, 0x8913, info);
        print_kv("  SIOCGIFADDR ioctl: ", ioret);

        kenux_net_detach((int)nfd);
    }
    kex_puts("");

    kex_puts("--- Event FD Test ---");
    long efd = sys_eventfd2(0, 0);
    print_kv("  eventfd(): ", efd);
    if (efd >= 0) {
        unsigned long val = 42;
        sys_write((int)efd, &val, 8);
        unsigned long rval = 0;
        sys_read((int)efd, &rval, 8);
        print_kv("  eventfd read value: ", (long)rval);
        sys_close((int)efd);
    }
    kex_puts("");

    kex_puts("--- Timer FD Test ---");
    long tfd = sys_timerfd_create(0, 0);
    print_kv("  timerfd_create(): ", tfd);
    if (tfd >= 0) {
        struct {
            long interval_sec;
            long interval_nsec;
            long value_sec;
            long value_nsec;
        } its = { 1, 0, 1, 0 };
        long sret = sys_timerfd_settime((int)tfd, 0, &its, 0);
        print_kv("  timerfd_settime(1s): ", sret);
        sys_close((int)tfd);
    }
    kex_puts("");

    kex_puts("========================================");
    kex_puts("  Network status test complete.");
    kex_puts("========================================");

    sys_exit(0);
}
