#include "uipc/uipc.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/poll.h>
#include <unistd.h>
#include <poll.h>

typedef struct {
    int fd;
    struct sockaddr_un addr;
} _Server;

typedef struct {
    int fd;
    char pathname[sizeof(((struct sockaddr_un*)0)->sun_path)];
} _Connection;

void uipc_free(void *ptr) {
    free(ptr);
}

uint32_t read_u32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
            ((uint32_t)buf[1] << 16) |
            ((uint32_t)buf[2] << 8)  |
            (uint32_t)buf[3];
}

#define write32(buf, value) { \
    (buf)[0] = (uint8_t)((value) >> 24); \
    (buf)[1] = (uint8_t)((value) >> 16); \
    (buf)[2] = (uint8_t)((value) >> 8); \
    (buf)[3] = (uint8_t)(value); \
}

void write_u32(uint8_t *buf, uint32_t value) {
    buf[0] = (uint8_t)(value >> 24);
    buf[1] = (uint8_t)(value >> 16);
    buf[2] = (uint8_t)(value >> 8);
    buf[3] = (uint8_t)value;
}

Server *server_create(const char* pathname) {
    if (!pathname || strlen(pathname) >= sizeof(((struct sockaddr_un*)0)->sun_path)) {
        errno = ENAMETOOLONG;
        return NULL;
    }
    _Server *srv = calloc(1, sizeof(_Server));
    if (!srv) return NULL;
    srv->fd = socket(AF_UNIX, SOCK_STREAM, 0);

    if (srv->fd < 0) { free(srv); return NULL; }
    srv->addr.sun_family = AF_UNIX;
    strcpy(srv->addr.sun_path, pathname);

    // Bind the address to the socket
    int status = bind(srv->fd, (const struct sockaddr*)&srv->addr, sizeof(srv->addr));
    if (status != 0) {
        fprintf(stderr, "ERROR: Failed to bind '%s' as an address for the socket %d: %s\n", pathname, srv->fd, strerror(errno));
        close(srv->fd);
        free(srv);
        return NULL;
    }

    struct sockaddr_un bound = {0};
    bound.sun_family = AF_UNIX;
    socklen_t size = sizeof(bound);
    status = getsockname(srv->fd, (struct sockaddr*)&bound, &size);
    if (status != 0) {
        fprintf(stderr, "ERROR: Failed to retrieve bound address of the %d socket: %s\n", srv->fd, strerror(errno));
        close(srv->fd);
        unlink(pathname);
        free(srv);
        return NULL;
    }

    if (strcmp(bound.sun_path, pathname) != 0) {
        fprintf(stderr, "ERROR: Failed to bind specified address for socket %d: %s\n", srv->fd, strerror(errno));
        close(srv->fd);
        unlink(pathname);
        free(srv);
        return NULL;
    }

    return (Server*)srv;
}

const char* server_address(Server* srv) {
    return ((_Server*)srv)->addr.sun_path;
}

bool server_listen(const Server* srv) {
    _Server *server = (_Server*)srv;
    int status = listen(server->fd, 1);
    if (status != 0) {
        fprintf(stderr, "ERROR: Failed to start listening on %s: %s\n", server->addr.sun_path, strerror(errno));
        server_close((Server*)srv);
        return false;
    }

    return true;
}

Connection* server_accept(Server* srv) {
    _Server *server = (_Server*)srv;
    _Connection *conn = calloc(1, sizeof(_Connection));
    if (!conn) return NULL;

    struct pollfd fd = {
        .fd = server->fd,
        .events = POLLIN,
        .revents = 0,
    };

    int pollstatus = poll(&fd, 1, -1);
    if (pollstatus <= 0) {
        fprintf(stderr, "ERROR: Failed to poll on the server socket");
        if (pollstatus == -1) {
            fprintf(stderr, ": %s", strerror(errno));
        }
        fprintf(stderr, "\n");
        free(conn);
        return NULL;
    }
    assert(fd.revents != 0);

    struct sockaddr_un client_addr = {0};
    client_addr.sun_family = AF_UNIX;
    socklen_t client_addr_size = sizeof(client_addr);
    conn->fd = accept(server->fd, (struct sockaddr*)&client_addr, &client_addr_size);
    if (conn->fd == -1) {
        fprintf(stderr, "ERROR: Failed to accept connection from client socket: %s\n", strerror(errno));
        free(conn);
        return NULL;
    }

    return (Connection*)conn;
}

void server_close(Server* srv) {
    _Server *server = (_Server*)srv;
    if (server && server->fd >= 0) {
        close(server->fd);
        server->fd = -1;
        unlink(server->addr.sun_path);
    }
}

void connection_close(Connection *conn) {
    _Connection *connection = (_Connection*)conn;
    if (connection && connection->fd >= 0) {
        close(connection->fd);
        connection->fd = -1;
    }
}

Connection *connection_allocate(const char* pathname) {
    _Connection *conn = calloc(1, sizeof(_Connection));
    if (!conn) return NULL;
    assert(conn != NULL);

    conn->fd = -1;
    if (!pathname || strlen(pathname) >= sizeof(conn->pathname)) {
        free(conn);
        errno = ENAMETOOLONG;
        return NULL;
    }
    strcpy(conn->pathname, pathname);

    return (Connection*)conn;
}

Connection *connection_from_socket(int fd, const char* addr) {
    _Connection *conn = (_Connection*)connection_allocate(addr);

    if (!conn) return NULL;
    conn->fd = fd;
    assert(strcmp(conn->pathname, addr) == 0);

    return (Connection*)conn;
}

bool connection_connect(const Connection *connection) {
    _Connection* conn = (_Connection*)connection;
    assert(conn->fd == -1 && "connection already bound");

    conn->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (conn->fd == -1) {
        fprintf(stderr, "ERROR: Failed to create a client socket: %s\n", strerror(errno));
        return false;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, conn->pathname);
    int status = connect(conn->fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (status == -1) {
        fprintf(stderr, "ERROR: Failed to connect to '%s': %s\n", conn->pathname, strerror(errno));
        close(conn->fd);
        conn->fd = -1;
        return false;
    }

    return true;
}

int connection_recv(Connection *conn, void *buf, size_t capacity) {
    _Connection *connection = (_Connection*)conn;
    int len;
    do { len = recv(connection->fd, buf, capacity, 0); } while (len < 0 && errno == EINTR);
    if (len == -1) {
        fprintf(stderr, "ERROR: Failed to read from client socket: %s\n", strerror(errno));
        connection_close(conn);
        return -1;
    }

    return len;
}

int connection_recv_bytes(Connection *conn, void *buf, size_t exact_size) {
    size_t bytes_read = 0;
    while (bytes_read < exact_size) {
        int n = connection_recv(conn, (uint8_t*)buf + bytes_read, exact_size - bytes_read);
        if (n <= 0) return -1;
        bytes_read += (size_t)n;
    }
    return (int)bytes_read;
}

Message* connection_recv_message(Connection *conn) {
    uint8_t header[4];
    if (connection_recv_bytes(conn, header, sizeof(header)) < 0) return NULL;
    uint32_t length = read_u32(header);
    if (length < 6 || length > 16 * 1024 * 1024) return NULL;
    uint8_t *buf = malloc(length);
    if (!buf) return NULL;
    if (connection_recv_bytes(conn, buf, length) < 0 || buf[0] > UIPC_VERSION) {
        free(buf);
        return NULL;
    }
    Message *msg = message_new(buf[0], buf[1], read_u32(buf + 2), buf + 6, length - 6);
    free(buf);
    return msg;
}

int connection_send(Connection *conn, void *buf, size_t size) {
    _Connection *connection = (_Connection*)conn;
    size_t sent = 0;
    while (sent < size) {
        ssize_t n = send(connection->fd, (uint8_t*)buf + sent, size - sent, MSG_NOSIGNAL);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return -1;
        sent += (size_t)n;
    }
    return (int)sent;
}

int connection_send_message(Connection *conn, const Message* message) {
    size_t msg_size = 4 + message->length;
    uint8_t *buf = malloc(msg_size);
    if (!buf) return -1;
    write_u32(buf, message->length);
    buf[4] = message->version;
    buf[5] = message->type;
    write_u32(buf + 6, message->request_id);
    memcpy(buf + 10, message->payload, message->length - 6);
    int result = connection_send(conn, buf, msg_size);
    free(buf);
    return result;
}

Message* message_new(
    uint8_t version,
    uint8_t type,
    uint32_t request_id,
    const uint8_t *payload,
    size_t payload_size
) {
    assert(version <= UIPC_VERSION && "unsupported UIPC message protocol version");

    Message* msg = malloc(sizeof(*msg) + payload_size);

    if (!msg) {
        return NULL;
    }

    uint32_t length = (uint32_t)(6 + payload_size);
    msg->length = length;
    msg->version = version;
    msg->type = type;
    msg->request_id = request_id;

    if (payload_size)
        memcpy(msg->payload, payload, payload_size);

    return msg;
}

const uint8_t *message_payload(const Message *msg, size_t *len) {
    assert((int32_t)msg->length - 6 >= 0);
    *len = msg->length - 6;
    if (*len == 0) {
        return NULL;
    }
    return msg->payload;
}

Message* request_new(const uint8_t *payload, size_t payload_size) {
    static uint32_t request_id = 0;
    return message_new(UIPC_VERSION, MSG_REQUEST, ++request_id, payload, payload_size);
}

Message* response_new(const Message* request, const uint8_t *payload, size_t payload_size) {
    return message_new(request->version, MSG_RESPONSE, request->request_id, payload, payload_size);
}

// Packet *packet_new() {
//     _Packet *pck = (_Packet*)malloc(sizeof(_Packet));
//     pck->root = NULL;
// }
