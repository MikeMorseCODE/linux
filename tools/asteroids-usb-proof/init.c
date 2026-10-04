#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>

#define G "/sys/kernel/config/usb_gadget/asteroids"

static int mk(const char *path)
{
    if (!mkdir(path, 0755))
        return 0;
    struct stat st;
    return errno == EEXIST && !stat(path, &st) && S_ISDIR(st.st_mode) ? 0 : -1;
}

/* ConfigFS attributes require one complete write, including the 0x prefix. */
static int wr(const char *path, const char *value)
{
    int fd = open(path, O_WRONLY | O_CLOEXEC);
    if (fd < 0)
        return -1;
    size_t len = strlen(value);
    ssize_t n;
    do { n = write(fd, value, len); } while (n < 0 && errno == EINTR);
    int result = n == (ssize_t)len ? 0 : -1;
    if (close(fd))
        result = -1;
    return result;
}

static void mount_wait(const char *type, const char *path)
{
    for (;;) {
        if (!mk(path) && (!mount(type, path, type, 0, "") || errno == EBUSY))
            return;
        sleep(1);
    }
}

static int first_udc(char *name, size_t size)
{
    DIR *dir = opendir("/sys/class/udc");
    if (!dir)
        return -1;
    struct dirent *entry;
    int result = -1;
    while ((entry = readdir(dir))) {
        struct stat st;
        if (entry->d_name[0] == '.' || strlen(entry->d_name) >= size)
            continue;
        /* Follow the class symlink; ignore stale entries and plain files. */
        if (fstatat(dirfd(dir), entry->d_name, &st, 0) || !S_ISDIR(st.st_mode))
            continue;
        strcpy(name, entry->d_name);
        result = 0;
        break;
    }
    closedir(dir);
    return result;
}

static int setup(void)
{
    if (mk(G) || wr(G "/idVendor", "0x1d6b") ||
        wr(G "/idProduct", "0x0104") || mk(G "/strings/0x409") ||
        wr(G "/strings/0x409/serialnumber", "USERSPACE-EXEC-OK") ||
        wr(G "/strings/0x409/manufacturer", "Asteroids") ||
        wr(G "/strings/0x409/product", "Asteroids Userspace Proof") ||
        mk(G "/configs/c.1") || mk(G "/functions/acm.usb0"))
        return -1;
    if (symlink(G "/functions/acm.usb0", G "/configs/c.1/acm.usb0") &&
        errno != EEXIST)
        return -1;
    return 0;
}

int main(void)
{
    mount_wait("devtmpfs", "/dev");
    mount_wait("proc", "/proc");
    mount_wait("sysfs", "/sys");
    mount_wait("configfs", "/sys/kernel/config");
    char udc[256];
    for (;;) {
        if (!first_udc(udc, sizeof(udc)) && !setup() && !wr(G "/UDC", udc))
            break;
        sleep(1);
    }
    for (;;)
        sleep(60);
}
