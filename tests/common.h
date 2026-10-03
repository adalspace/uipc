#pragma once
#include <uipc/uipc.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <dirent.h>
#include <time.h>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: CHECK(%s) failed (errno=%d)\n", __FILE__, __LINE__, #expr, errno); exit(1); } } while (0)

static char test_dir[] = "/tmp/uipc-tests-XXXXXX";
static char test_path[108];
static void cleanup_path(void) { unlink(test_path); rmdir(test_dir); }
static void setup_path(void) {
    CHECK(mkdtemp(test_dir) != NULL);
    CHECK(snprintf(test_path, sizeof(test_path), "%s/socket", test_dir) > 0);
    CHECK(atexit(cleanup_path) == 0);
}
static void release_connection(Connection *c) { connection_close(c); uipc_free(c); }
static void release_server(Server *s) { server_close(s); uipc_free(s); }
static int fd_count(void) {
    DIR *dir = opendir("/proc/self/fd");
    if (!dir) return -1;
    int count = 0;
    while (readdir(dir)) ++count;
    closedir(dir);
    return count;
}
static void raw_send(int fd, const void *data, size_t size) {
    const unsigned char *p = (const unsigned char*)data;
    while (size) {
        ssize_t n = send(fd, p, size, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        CHECK(n > 0);
        p += n; size -= (size_t)n;
    }
}
static void raw_read(int fd, void *data, size_t size) {
    unsigned char *p = (unsigned char*)data;
    while (size) {
        ssize_t n = recv(fd, p, size, 0);
        if (n < 0 && errno == EINTR) continue;
        CHECK(n > 0);
        p += n; size -= (size_t)n;
    }
}
static double now_seconds(void) {
    struct timespec t;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &t) == 0);
    return t.tv_sec + t.tv_nsec / 1e9;
}
