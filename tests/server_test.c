#include "common.h"
#include <sys/wait.h>

static void exchange(unsigned count) {
    setup_path();
    int before = fd_count();
    Server *s = server_create(test_path);
    CHECK(s && !strcmp(server_address(s), test_path));
    CHECK(server_listen(s));
    pid_t child = fork();
    CHECK(child >= 0);
    if (!child) {
        /* Independent raw peer verifies exact wire bytes, including empty payload. */
        for (unsigned i = 0; i < count; ++i) {
            int fd = socket(AF_UNIX, SOCK_STREAM, 0);
            CHECK(fd >= 0);
            struct sockaddr_un addr = {0};
            addr.sun_family = AF_UNIX;
            strcpy(addr.sun_path, test_path);
            CHECK(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
            unsigned char frame[] = {0,0,0,6,1,1,0,0,0,42};
            raw_send(fd, frame, sizeof(frame));
            unsigned char response[10];
            raw_read(fd, response, sizeof(response));
            frame[5] = MSG_RESPONSE;
            CHECK(!memcmp(frame, response, sizeof(frame)));
            close(fd);
        }
        _exit(0);
    }
    double start = now_seconds();
    for (unsigned i = 0; i < count; ++i) {
        Connection *c = server_accept(s);
        CHECK(c);
        Message *m = connection_recv_message(c);
        CHECK(m && m->length == 6 && m->request_id == 42 && m->type == MSG_REQUEST);
        Message *reply = response_new(m, NULL, 0);
        CHECK(reply && connection_send_message(c, reply) == 10);
        uipc_free(reply); uipc_free(m); release_connection(c);
    }
    int status;
    CHECK(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    release_server(s);
    CHECK(access(test_path, F_OK) == -1);
    if (before >= 0) CHECK(fd_count() == before);
    printf("server: %u accepts in %.3fs\n", count, now_seconds() - start);
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    if (!strcmp(argv[1], "exchange")) exchange(1);
    else if (!strcmp(argv[1], "stress")) exchange(2000);
    else if (!strcmp(argv[1], "lifecycle")) {
        setup_path(); int before = fd_count();
        for (int i = 0; i < 200; ++i) {
            Server *s = server_create(test_path); CHECK(s);
            CHECK(server_listen(s));
            CHECK(server_create(test_path) == NULL);
            server_close(s); server_close(s); uipc_free(s);
            CHECK(access(test_path, F_OK) == -1);
        }
        if (before >= 0) CHECK(fd_count() == before);
    } else if (!strcmp(argv[1], "concurrent")) {
        setup_path(); Server *s = server_create(test_path); CHECK(s && server_listen(s));
        pid_t children[8];
        for (int i = 0; i < 8; ++i) {
            children[i] = fork(); CHECK(children[i] >= 0);
            if (!children[i]) {
                int fd = socket(AF_UNIX, SOCK_STREAM, 0); CHECK(fd >= 0);
                struct sockaddr_un addr = {0}; addr.sun_family = AF_UNIX; strcpy(addr.sun_path, test_path);
                CHECK(connect(fd, (struct sockaddr*)&addr, sizeof(addr)) == 0);
                unsigned char frame[] = {0,0,0,6,1,1,0,0,0,0}; frame[9] = (unsigned char)i;
                raw_send(fd, frame, sizeof(frame)); close(fd); _exit(0);
            }
        }
        unsigned seen = 0;
        for (int i = 0; i < 8; ++i) {
            Connection *c = server_accept(s); CHECK(c); Message *m = connection_recv_message(c);
            CHECK(m && m->request_id < 8 && !(seen & (1u << m->request_id)));
            seen |= 1u << m->request_id; uipc_free(m); release_connection(c);
        }
        CHECK(seen == 255);
        for (int i = 0; i < 8; ++i) {
            int status; CHECK(waitpid(children[i], &status, 0) == children[i] && WIFEXITED(status) && WEXITSTATUS(status) == 0);
        }
        release_server(s);
    } else if (!strcmp(argv[1], "paths")) {
        char long_path[256]; memset(long_path, 'x', sizeof(long_path)); long_path[255] = 0;
        CHECK(server_create(long_path) == NULL);
        CHECK(server_create("/no-such-uipc-parent/socket") == NULL);
    } else CHECK(0);
    return 0;
}
