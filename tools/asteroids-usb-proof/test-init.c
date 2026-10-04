#define _GNU_SOURCE
#include <assert.h>
#include <setjmp.h>
#include <stdlib.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <unistd.h>
static int fake_mount(const char *, const char *, const char *, unsigned long, const void *);
static int fake_mkdir(const char *, mode_t);
static int fake_open(const char *, int, ...);
static ssize_t fake_write(int, const void *, size_t);
static int fake_close(int);
static int fake_symlink(const char *, const char *);
static DIR *fake_opendir(const char *);
static unsigned fake_sleep(unsigned);
#define mount fake_mount
#define mkdir fake_mkdir
#define open fake_open
#define write fake_write
#define close fake_close
#define symlink fake_symlink
#define opendir fake_opendir
#define sleep fake_sleep
#define main proof_main
#include "init.c"
#undef mount
#undef mkdir
#undef open
#undef write
#undef close
#undef symlink
#undef opendir
#undef sleep
#undef main
static jmp_buf done;
static int ticks, binds, mounts, ids, strings;
static char dirpath[] = "/tmp/asteroids-udc-test-XXXXXX";
static char attr[512];
static int fake_mount(const char *a, const char *b, const char *c, unsigned long d, const void *e)
{ (void)a;(void)b;(void)c;(void)d;(void)e; ++mounts; if (mounts == 1) {errno=EINTR;return -1;} return 0; }
static int fake_mkdir(const char *p, mode_t m)
{ (void)m; if (!strncmp(p,G,strlen(G))) assert(ticks>=6); return 0; }
static int fake_open(const char *p, int f, ...)
{ (void)f; snprintf(attr,sizeof(attr),"%s",p); return 123; }
static ssize_t fake_write(int f, const void *b, size_t n)
{
 (void)f;
 if (strstr(attr,"idVendor")) {assert(n==6 && !memcmp(b,"0x1d6b",6));++ids;}
 if (strstr(attr,"idProduct")) {assert(n==6 && !memcmp(b,"0x0104",6));++ids;}
 if (strstr(attr,"serialnumber")) {assert(n==17 && !memcmp(b,"USERSPACE-EXEC-OK",17));++strings;}
 if (!strcmp(attr,G "/UDC")) {assert(n==11 && !memcmp(b,"a600000.usb",11)); if (++binds==1) {errno=EBUSY;return -1;}}
 return n;
}
static int fake_close(int f) { (void)f;return 0; }
static int fake_symlink(const char *a,const char *b)
{ assert(!strcmp(a,G "/functions/acm.usb0"));assert(!strcmp(b,G "/configs/c.1/acm.usb0"));return 0; }
static DIR *fake_opendir(const char *p) { assert(!strcmp(p,"/sys/class/udc"));return opendir(dirpath); }
static unsigned fake_sleep(unsigned s)
{
 if (s==60) { assert(binds==2 && ticks>=7 && mounts==5 && ids==4 && strings==2);longjmp(done,1); }
 assert(s==1 && ticks<20);
 if (++ticks==6) {char p[512];snprintf(p,sizeof(p),"%s/a600000.usb",dirpath);assert(!mkdir(p,0755));}
 return 0;
}
int main(void)
{
 assert(mkdtemp(dirpath));
 if (!setjmp(done)) proof_main();
 char p[512];snprintf(p,sizeof(p),"%s/a600000.usb",dirpath);rmdir(p);rmdir(dirpath);
 puts("PASS: mount retry, late UDC, exact descriptors, failed-bind retry, permanent PID1");
}
