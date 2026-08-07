#include "kex_user.h"

struct kex_stat_buf {
    unsigned long st_dev;
    unsigned long st_ino;
    unsigned long st_nlink;
    int st_mode;
    int st_uid;
    int st_gid;
    unsigned long st_rdev;
    unsigned long st_size;
    unsigned long st_blksize;
    unsigned long st_blocks;
    unsigned long st_atime;
    unsigned long st_mtime;
    unsigned long st_ctime;
};

struct kex_dirent {
    unsigned long d_ino;
    unsigned long d_off;
    unsigned short d_reclen;
    unsigned char d_type;
    char d_name[256];
};

static void print_str(const char *s) {
    sys_write(1, s, kex_strlen(s));
}

static void print_kv(const char *key, long val) {
    print_str(key);
    kex_printlong(val, 0);
    print_str("\n");
}

static void print_mode(int mode) {
    char buf[11];
    buf[0] = (mode & 0x40000) ? 'd' : '-';
    buf[1] = (mode & 0x100) ? 'r' : '-';
    buf[2] = (mode & 0x80) ? 'w' : '-';
    buf[3] = (mode & 0x40) ? 'x' : '-';
    buf[4] = (mode & 0x40) ? 'r' : '-';
    buf[5] = (mode & 0x20) ? 'w' : '-';
    buf[6] = (mode & 0x10) ? 'x' : '-';
    buf[7] = (mode & 0x4) ? 'r' : '-';
    buf[8] = (mode & 0x2) ? 'w' : '-';
    buf[9] = (mode & 0x1) ? 'x' : '-';
    buf[10] = '\0';
    print_str(buf);
}

static void print_size(unsigned long sz) {
    if (sz < 1024) {
        kex_printlong((long)sz, 0);
        print_str(" B");
    } else if (sz < 1024 * 1024) {
        kex_printlong((long)(sz / 1024), 0);
        print_str(" KB");
    } else if (sz < 1024UL * 1024 * 1024) {
        kex_printlong((long)(sz / (1024 * 1024)), 0);
        print_str(" MB");
    } else {
        kex_printlong((long)(sz / (1024UL * 1024 * 1024)), 0);
        print_str(" GB");
    }
}

void _start(void) {
    kex_puts("========================================");
    kex_puts("  Kenux File Browser (filebrowser)     ");
    kex_puts("========================================");
    kex_puts("");

    char cwd[512];
    kex_memset(cwd, 0, sizeof(cwd));
    long ret = sys_getcwd(cwd, sizeof(cwd));
    if (ret > 0) {
        print_str("  Current directory: ");
        print_str(cwd);
        kex_putchar('\n');
    } else {
        print_kv("  getcwd failed: ", ret);
    }
    kex_puts("");

    kex_puts("--- Directory Listing ---");
    int dirfd = (int)sys_open(".", 0x10000, 0);
    if (dirfd >= 0) {
        char dirbuf[4096];
        int entries = 0;
        while (1) {
            long nread = sys_getdents64(dirfd, dirbuf, sizeof(dirbuf));
            if (nread <= 0) break;
            long pos = 0;
            while (pos < nread) {
                struct kex_dirent *d = (struct kex_dirent *)(dirbuf + pos);
                if (d->d_ino != 0) {
                    print_str("  [");
                    char typech = '?';
                    switch (d->d_type) {
                        case 1: typech = 'F'; break;
                        case 2: typech = 'D'; break;
                        case 4: typech = 'L'; break;
                        case 8: typech = '-'; break;
                        case 10: typech = 'L'; break;
                        default: typech = '?'; break;
                    }
                    kex_putchar(typech);
                    print_str("] ");
                    print_str(d->d_name);
                    kex_putchar('\n');
                    entries++;
                }
                if (d->d_reclen == 0) break;
                pos += d->d_reclen;
            }
        }
        print_str("  Total entries: ");
        kex_printlong(entries, 1);
        sys_close(dirfd);
    } else {
        print_kv("  opendir failed: ", dirfd);
    }
    kex_puts("");

    kex_puts("--- File Stat Test ---");
    struct kex_stat_buf st;
    kex_memset(&st, 0, sizeof(st));
    ret = sys_stat(".", &st);
    if (ret == 0) {
        kex_puts("  Stat of current directory:");
        print_str("    mode:  ");
        print_mode(st.st_mode);
        kex_putchar('\n');
        print_str("    size:  ");
        print_size(st.st_size);
        kex_putchar('\n');
        print_kv("    nlink: ", (long)st.st_nlink);
        print_kv("    uid:   ", (long)st.st_uid);
        print_kv("    gid:   ", (long)st.st_gid);
    } else {
        print_kv("  stat failed: ", ret);
    }
    kex_puts("");

    kex_puts("--- File Write Test ---");
    int fd = (int)sys_open("testfile.txt", 0x241, 0644);
    if (fd >= 0) {
        const char *content = "Hello from KenuxOS file browser!\nThis file was created by a Kex application.\n";
        long wlen = sys_write(fd, content, kex_strlen(content));
        print_str("  Written ");
        kex_printlong(wlen, 0);
        print_str(" bytes to testfile.txt\n");
        sys_close(fd);

        fd = (int)sys_open("testfile.txt", 0, 0);
        if (fd >= 0) {
            char readbuf[256];
            kex_memset(readbuf, 0, sizeof(readbuf));
            long rlen = sys_read(fd, readbuf, sizeof(readbuf) - 1);
            print_str("  Read back ");
            kex_printlong(rlen, 0);
            print_str(" bytes:\n    ");
            print_str(readbuf);
            sys_close(fd);
        }

        struct kex_stat_buf fst;
        kex_memset(&fst, 0, sizeof(fst));
        ret = sys_stat("testfile.txt", &fst);
        if (ret == 0) {
            kex_puts("  File stat after write:");
            print_str("    mode:  ");
            print_mode(fst.st_mode);
            kex_putchar('\n');
            print_str("    size:  ");
            print_size(fst.st_size);
            kex_putchar('\n');
        }

        sys_unlink("testfile.txt");
        kex_puts("  Cleaned up testfile.txt");
    } else {
        print_kv("  file create failed: ", fd);
    }
    kex_puts("");

    kex_puts("--- Directory Create Test ---");
    ret = sys_mkdir("kenux_testdir", 0755);
    if (ret == 0) {
        kex_puts("  Created kenux_testdir/");
        ret = sys_rmdir("kenux_testdir");
        if (ret == 0) {
            kex_puts("  Removed kenux_testdir/");
        } else {
            print_kv("  rmdir failed: ", ret);
        }
    } else {
        print_kv("  mkdir result (may exist): ", ret);
    }
    kex_puts("");

    kex_puts("--- Filesystem Stats ---");
    struct {
        long f_type;
        long f_bsize;
        unsigned long f_blocks;
        unsigned long f_bfree;
        unsigned long f_bavail;
        unsigned long f_files;
        unsigned long f_ffree;
    } statfs_buf;
    kex_memset(&statfs_buf, 0, sizeof(statfs_buf));
    ret = sys_statfs(".", &statfs_buf);
    if (ret == 0) {
        kex_puts("  Filesystem info:");
        print_kv("    block size:  ", (long)statfs_buf.f_bsize);
        print_kv("    total blocks: ", (long)statfs_buf.f_blocks);
        print_kv("    free blocks:  ", (long)statfs_buf.f_bfree);
        print_kv("    total files:  ", (long)statfs_buf.f_files);
        print_kv("    free inodes:  ", (long)statfs_buf.f_ffree);
    } else {
        print_kv("  statfs failed: ", ret);
    }
    kex_puts("");

    kex_puts("========================================");
    kex_puts("  File browser test complete.");
    kex_puts("========================================");

    sys_exit(0);
}
