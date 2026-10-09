#pragma once
#include <sys/socket.h>
#include <sys/un.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

#define UIPC_VERSION 1

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Server Server;
typedef struct Connection Connection;

typedef uint8_t MessageType;
#define MSG_REQUEST 1
#define MSG_RESPONSE 2
#define MSG_EVENT 3

typedef struct {
    uint32_t length;
    uint8_t version;
    MessageType type;
    uint32_t request_id;
    uint8_t payload[];
} Message;

typedef const Message *(*message_handler)(Message *request, void *arg);

uint32_t read_u32(const uint8_t *buf);
void write_u32(uint8_t *buf, uint32_t value);

Server *server_create(const char *pathname);
const char *server_address(Server *srv);
void server_register_handler(Server *srv, message_handler handler, void *arg);
bool server_listen(Server *srv);
// Connection *server_accept(Server *srv);
void server_close(Server *srv);

Connection *connection_allocate(const char *pathname);
Connection *connection_from_socket(int fd, const char *pathname);
bool connection_connect(const Connection *conn);
int connection_recv(Connection *conn, void *buf, size_t capacity);
int connection_recv_bytes(Connection *conn, void *buf, size_t exact_size);
int connection_send(Connection *conn, void *buf, size_t size);
Message *connection_recv_message(Connection *conn);
int connection_send_message(Connection *conn, const Message *message);
void connection_close(Connection *conn);

Message *message_new(uint8_t version, MessageType type, uint32_t request_id, const uint8_t *payload, size_t payload_size);
const uint8_t *message_payload(const Message *msg, size_t *len);
Message *request_new(const uint8_t *payload, size_t payload_size);
Message *response_new(const Message *request, const uint8_t *payload, size_t payload_size);

void uipc_free(void *ptr);

#ifdef __cplusplus
} // extern "C"
#endif
