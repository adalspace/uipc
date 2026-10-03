#pragma once

#include "uipc.h"
#include <cpl-basics/def.hpp>
#include <cpl-basics/string.hpp>
#include <cstdint>
#include <cstring>
#include <stdexcept>

namespace uipc {

enum class message_type : Uint8 {
    REQUEST = 1,
    RESPONSE = 2,
    EVENT = 3,
}; // enum class message_type

class request;
class response;

class message {
public:
    message(uint8_t version, message_type type, uint32_t request_id, const uint8_t *payload, size_t payload_size)
    {
        if (payload_size > 0) {
          M_payload.ResizeUninitialized(payload_size);
          std::memcpy(M_payload.Data(), payload, payload_size);
        }
        M_message = message_new(version, static_cast<::MessageType>(type), request_id, M_payload.Data(), M_payload.Size());
    }

    message(::Message *msg)
        : M_message(msg)
    {
        if (!msg) {
          throw std::runtime_error("constructing message from nullptr");
        }
        size_t payload_size = 0;
        const uint8_t *payload = message_payload(msg, &payload_size);
        if (payload && payload_size > 0) {
            M_payload.ResizeUninitialized(payload_size);
            std::memcpy(M_payload.Data(), payload, payload_size);
        }
    }

    message(const message& other)
        : message(other.version(), other.type(), other.request_id(), other.payload().Data(), other.payload().Size()) {}
    message operator=(const message&) = delete;

    virtual ~message()
    {
        uipc_free(M_message);
    }
public:
    [[nodiscard]] uint32_t length() const noexcept { return M_message->length; }
    [[nodiscard]] uint8_t version() const noexcept { return M_message->version; }
    [[nodiscard]] message_type type() const noexcept { return static_cast<message_type>(M_message->type); }
    [[nodiscard]] uint32_t request_id() const noexcept { return M_message->request_id; }
    [[nodiscard]] const Array<Byte>& payload() const noexcept { return M_payload; };

    const Message *raw_message() const noexcept { return M_message; }
private:
    ::Message *M_message;
    Array<Byte> M_payload;
    friend uipc::request;
    friend uipc::response;
}; // class message

class request : public message {
public:
    request(uint8_t version, uint32_t request_id, const Byte *payload, size_t payload_size)
        : message(version, message_type::REQUEST, request_id, payload, payload_size) { }

    static request from_message(const message &msg)
    {
        assert(msg.type() == message_type::REQUEST);
        return request(msg.version(), msg.request_id(), msg.payload().Data(), msg.payload().Size());
    }
    ~request() = default;
public:
    static uint32_t new_request_id() { return ++S_request_id; }
public:
    inline static Uint32 S_request_id;
}; // request

class response : public message {
private:
    response(uint8_t version, uint32_t request_id, const uint8_t *payload, size_t payload_size)
        : message(version, message_type::RESPONSE, request_id, payload, payload_size) { }
public:
    static response from_request(const request &req, const uint8_t *payload, size_t payload_size)
    {
        return response(req.version(), req.request_id(), payload, payload_size);
    }
    ~response() = default;
}; // response

class server;

class connection {
private:
    connection(::Connection *conn)
        : M_connection(conn) { }
public:
    connection(const String &pathname)
        : M_connection(connection_allocate(pathname.c_str())) { }

    static connection from_socket(int fd, const String &pathname)
    {
        return connection(connection_from_socket(fd, pathname.c_str()));
    }

    connection(const connection&) = delete;
    connection operator=(const connection&) = delete;

    virtual ~connection()
    {
        close();
        uipc_free(M_connection);
    }
public:
    bool connect() const { return connection_connect(M_connection); }
    int read(void *buf, size_t capacity) const { return connection_recv(M_connection, buf, capacity); }
    int read_bytes(void *buf, size_t exact_size) const { return connection_recv_bytes(M_connection, buf, exact_size); }
    message read_message() const { return message(connection_recv_message(M_connection)); }
    int send_message(const message &msg) const { return connection_send_message(M_connection, msg.raw_message()); }
    void close() const { connection_close(M_connection); }
private:
    int send(void *buf, size_t size) const { return connection_send(M_connection, buf, size); }
private:
    ::Connection *M_connection;
    friend class uipc::server;
}; // class connection

class server {
public:
    server(const String &pathname)
        : M_server(server_create(pathname.c_str())) { }
    server(const server&) = delete;
    server operator=(const server&) = delete;
    
    virtual ~server()
    {
      close();
      uipc_free(M_server);
    }
public:
    String address() const noexcept { return server_address(M_server); }
    bool listen() const { return server_listen(M_server); }
    connection accept_connection() { return connection(server_accept(M_server)); }
    void close() { server_close(M_server); }
private:
    ::Server *M_server;
}; // class server

} // namespace uipc
