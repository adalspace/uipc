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
    char *pathname;
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
    _Server *srv = malloc(sizeof(_Server));
    srv->fd = socket(AF_UNIX, SOCK_STREAM, 0);

    srv->addr.sun_family = AF_UNIX;
    strcpy(srv->addr.sun_path, pathname);

    // Bind the address to the socket
    int status = bind(srv->fd, (const struct sockaddr*)&srv->addr, sizeof(srv->addr));
    if (status != 0) {
        fprintf(stderr, "ERROR: Failed to bind '%s' as an address for the socket %d: %s\n", pathname, srv->fd, strerror(errno));
        close(srv->fd);
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
        return NULL;
    }

    if (strcmp(bound.sun_path, pathname) != 0) {
        fprintf(stderr, "ERROR: Failed to bind specified address for socket %d: %s\n", srv->fd, strerror(errno));
        close(srv->fd);
        unlink(pathname);
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
        close(server->fd);
        unlink(server->addr.sun_path);
        return false;
    }

    return true;
}

Connection* server_accept(Server* srv) {
    _Server *server = (_Server*)srv;
    _Connection *conn = malloc(sizeof(_Connection));

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
        return NULL;
    }
    assert(fd.revents != 0);

    struct sockaddr_un client_addr = {0};
    client_addr.sun_family = AF_UNIX;
    socklen_t client_addr_size = sizeof(client_addr);
    conn->fd = accept(server->fd, (struct sockaddr*)&client_addr, &client_addr_size);
    if (conn->fd == -1) {
        fprintf(stderr, "ERROR: Failed to accept connection from client socket: %s\n", strerror(errno));
        close(server->fd);
        unlink(server->addr.sun_path);
        return NULL;
    }

    return (Connection*)conn;
}

void server_close(Server* srv) {
    _Server *server = (_Server*)srv;
    close(server->fd);
    unlink(server->addr.sun_path);
}

void connection_close(Connection *conn) {
    _Connection *connection = (_Connection*)conn;
    close(connection->fd);
}

Connection *connection_allocate(const char* pathname) {
    _Connection *conn = malloc(sizeof(_Connection));
    assert(conn != NULL);

    conn->fd = -1;
    conn->pathname = malloc(strlen(pathname) * sizeof(char));
    assert(conn->pathname != NULL);
    strcpy(conn->pathname, pathname);

    return (Connection*)conn;
}

Connection *connection_from_socket(int fd, const char* addr) {
    _Connection *conn = (_Connection*)connection_allocate(addr);

    conn->fd = fd;
    assert(strcmp(conn->pathname, addr) == 0);

    return (Connection*)conn;
}

bool connection_connect(const Connection *connection) {
    _Connection* conn = (_Connection*)connection;
    assert(conn->fd == -1 && "connection already bound");
    assert(conn->pathname != NULL);

    conn->fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (conn->fd == -1) {
        fprintf(stderr, "ERROR: Failed to create a client socket: %s\n", strerror(errno));
        return NULL;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, conn->pathname);
    int status = connect(conn->fd, (const struct sockaddr*)&addr, sizeof(addr));
    if (status == -1) {
        fprintf(stderr, "ERROR: Failed to connect to '%s': %s\n", conn->pathname, strerror(errno));
        close(conn->fd);
        return false;
    }

    return true;
}

int connection_recv(Connection *conn, void *buf, size_t capacity) {
    _Connection *connection = (_Connection*)conn;
    int len = recv(connection->fd, buf, capacity, 0);
    if (len == -1) {
        fprintf(stderr, "ERROR: Failed to read from client socket: %s\n", strerror(errno));
        close(connection->fd);
        return -1;
    }

    return len;
}

int connection_recv_bytes(Connection *conn, void *buf, size_t exact_size) {
    size_t bytes_read = 0;
    do {
        int n = connection_recv(conn, (void*)((uint8_t*)buf + bytes_read), exact_size - bytes_read);
        bytes_read += n;
    } while (bytes_read < exact_size);

    assert(bytes_read == exact_size);
    return bytes_read;
}

Message* connection_recv_message(Connection *conn) {
    uint8_t *buf = malloc(4);
    int n = connection_recv_bytes(conn, buf, 4);
    uint32_t length = read_u32(buf);
    printf("DEBUG: Message length is %d bytes\n", length);
    buf = realloc(buf, length + 4);
    n = connection_recv_bytes(conn, buf + 4, length);
    printf("DEBUG: Read %d bytes of the message\n", n);
    uint8_t version = buf[4];
    MessageType type = buf[5];
    uint32_t request_id = read_u32(buf + 6);
    uint32_t payload_size = length - 6;
    uint8_t *payload = malloc(payload_size * sizeof(uint8_t));
    memcpy(payload, buf + 10, payload_size);
    Message *msg = message_new(version, type, request_id, payload, payload_size);
    uipc_free(buf);
    return msg;
}

int connection_send(Connection *conn, void *buf, size_t size) {
    _Connection *connection = (_Connection*)conn;
    ssize_t len = send(connection->fd, buf, size, 0);
    if (len == -1) {
        fprintf(stderr, "ERROR: Failed to send message to the client: %s\n", strerror(errno));
        return 1;
    }
    assert((size_t)len == size);
    return len;
}

int connection_send_message(Connection *conn, const Message* message) {
    size_t msg_size = 4 + message->length;
    uint8_t *buf = malloc(msg_size);
    write_u32(buf, message->length);
    buf[4] = message->version;
    buf[5] = message->type;
    write_u32(buf + 6, message->request_id);
    memcpy(buf + 10, message->payload, message->length - 6);
    connection_send(conn, buf, msg_size);
    free(buf);
    return msg_size;
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
