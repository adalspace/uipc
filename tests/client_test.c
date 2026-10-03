#include "common.h"
#include <pthread.h>
#include <sys/wait.h>

struct writer { int fd; const unsigned char *data; size_t size; size_t chunk; };
static void *write_peer(void *arg) {
    struct writer *w = (struct writer*)arg;
    for (size_t i = 0; i < w->size;) {
        size_t n = w->size - i; if (n > w->chunk) n = w->chunk;
        raw_send(w->fd, w->data + i, n); i += n;
    }
    shutdown(w->fd, SHUT_WR);
    return NULL;
}
static void frame_case(const unsigned char *frame, size_t size, int valid) {
    int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
    Connection *c = connection_from_socket(fds[0], "peer"); CHECK(c);
    struct writer w = {fds[1], frame, size, 1};
    pthread_t thread; CHECK(pthread_create(&thread, NULL, write_peer, &w) == 0);
    Message *m = connection_recv_message(c);
    if (valid) {
        CHECK(m && m->length == 9 && m->version == 1 && m->type == MSG_RESPONSE && m->request_id == 42);
        CHECK(!memcmp(m->payload, "a\0b", 3));
    } else CHECK(m == NULL);
    uipc_free(m); CHECK(pthread_join(thread, NULL) == 0);
    release_connection(c); close(fds[1]);
}
static void transfer(size_t size, unsigned count) {
    unsigned char *data = malloc(size); CHECK(data);
    for (size_t i = 0; i < size; ++i) data[i] = (unsigned char)(i * 31);
    int before = fd_count(); double start = now_seconds();
    for (unsigned j = 0; j < count; ++j) {
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        Connection *c = connection_from_socket(fds[0], "peer"); CHECK(c);
        struct writer w = {fds[1], data, size, 4096};
        pthread_t thread; CHECK(pthread_create(&thread, NULL, write_peer, &w) == 0);
        unsigned char *got = malloc(size); CHECK(got);
        CHECK(connection_recv_bytes(c, got, size) == (int)size);
        CHECK(!memcmp(data, got, size)); free(got);
        CHECK(pthread_join(thread, NULL) == 0);
        release_connection(c); close(fds[1]);
    }
    if (before >= 0) CHECK(fd_count() == before);
    printf("client: %.1f MiB in %.3fs\n", (double)size * count / 1048576, now_seconds() - start);
    free(data);
}
static void *read_peer(void *arg) {
    struct writer *w = (struct writer*)arg;
    unsigned char *got = malloc(w->size); CHECK(got);
    raw_read(w->fd, got, w->size);
    CHECK(!memcmp(got, w->data, w->size)); free(got); return NULL;
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    unsigned char good[] = {0,0,0,9,1,2,0,0,0,42,'a',0,'b'};
    if (!strcmp(argv[1], "fragmented")) frame_case(good, sizeof(good), 1);
    else if (!strcmp(argv[1], "truncated")) {
        for (size_t i = 0; i < sizeof(good); ++i) frame_case(good, i, 0);
    } else if (!strcmp(argv[1], "invalid_length")) {
        unsigned char small[] = {0,0,0,5}; frame_case(small, 4, 0);
        unsigned char huge[] = {255,255,255,255}; frame_case(huge, 4, 0);
        good[4] = UIPC_VERSION + 1; frame_case(good, sizeof(good), 0);
    } else if (!strcmp(argv[1], "stress")) transfer(1024 * 1024, 100);
    else if (!strcmp(argv[1], "send_large")) {
        size_t size = 2 * 1024 * 1024;
        unsigned char *data = malloc(size); CHECK(data); memset(data, 0xa5, size);
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        int capacity = 1024; CHECK(setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &capacity, sizeof(capacity)) == 0);
        Connection *c = connection_from_socket(fds[0], "peer"); CHECK(c);
        struct writer w = {fds[1], data, size, 4096}; pthread_t thread;
        CHECK(pthread_create(&thread, NULL, read_peer, &w) == 0);
        CHECK(connection_send(c, data, size) == (int)size);
        CHECK(pthread_join(thread, NULL) == 0); release_connection(c); close(fds[1]); free(data);
    } else if (!strcmp(argv[1], "closed_peer")) {
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        Connection *c = connection_from_socket(fds[0], "peer"); CHECK(c); close(fds[1]);
        unsigned char byte = 0;
        CHECK(connection_recv_bytes(c, &byte, 1) < 0);
        CHECK(connection_send(c, &byte, 1) < 0);
        release_connection(c);
    } else if (!strcmp(argv[1], "connect_retry")) {
        setup_path(); Connection *c = connection_allocate(test_path); CHECK(c);
        CHECK(!connection_connect(c));
        Server *s = server_create(test_path); CHECK(s && server_listen(s));
        CHECK(connection_connect(c)); Connection *accepted = server_accept(s); CHECK(accepted);
        release_connection(accepted); release_connection(c); release_server(s);
    } else if (!strcmp(argv[1], "connect_wire")) {
        setup_path();
        int listener = socket(AF_UNIX, SOCK_STREAM, 0); CHECK(listener >= 0);
        struct sockaddr_un addr = {0}; addr.sun_family = AF_UNIX; strcpy(addr.sun_path, test_path);
        CHECK(bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == 0 && listen(listener, 1) == 0);
        pid_t pid = fork(); CHECK(pid >= 0);
        if (!pid) {
            int fd = accept(listener, NULL, NULL); CHECK(fd >= 0);
            unsigned char got[13]; raw_read(fd, got, sizeof(got));
            unsigned char expected[] = {0,0,0,9,1,1,0,0,0,42,'a',0,'b'};
            CHECK(!memcmp(got, expected, sizeof(got)));
            expected[5] = MSG_RESPONSE; raw_send(fd, expected, sizeof(expected));
            close(fd); close(listener); _exit(0);
        }
        Connection *c = connection_allocate(test_path); CHECK(c && connection_connect(c));
        Message *m = message_new(1, MSG_REQUEST, 42, (const uint8_t*)"a\0b", 3); CHECK(m);
        CHECK(connection_send_message(c, m) == 13);
        Message *r = connection_recv_message(c); CHECK(r && r->type == MSG_RESPONSE && r->request_id == 42);
        CHECK(r->length == 9 && !memcmp(r->payload, "a\0b", 3));
        uipc_free(r); uipc_free(m); release_connection(c); close(listener);
        int status; CHECK(waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    } else if (!strcmp(argv[1], "messages")) {
        unsigned char data[] = {0,255,1};
        Message *m = request_new(data, sizeof(data)); CHECK(m);
        data[0] = 9; size_t len = 0;
        CHECK(message_payload(m, &len)[0] == 0 && len == 3);
        Message *r = response_new(m, NULL, 0); CHECK(r && r->request_id == m->request_id && r->type == MSG_RESPONSE);
        CHECK(message_payload(r, &len) == NULL && len == 0);
        Message *next = request_new(NULL, 0); CHECK(next && next->request_id > m->request_id);
        unsigned char endian[4]; write_u32(endian, 0x12345678); CHECK(!memcmp(endian, "\x12\x34\x56\x78", 4));
        CHECK(read_u32(endian) == 0x12345678);
        uipc_free(next); uipc_free(r); uipc_free(m);
    } else CHECK(0);
    return 0;
}
