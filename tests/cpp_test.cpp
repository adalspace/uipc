#include "common.h"
#include <uipc/uipc.hpp>
#include <uipc/upack.h>
#include <thread>
#include <fcntl.h>
#include <type_traits>
#include <vector>

template<typename T>
static void check_message_copy() {
    // Accept a safe deep copy or a deleted copy constructor.
    if constexpr (std::is_copy_constructible_v<T>) {
        unsigned char data = 7;
        T first(UIPC_VERSION, uipc::message_type::EVENT, 1, &data, 1);
        T copy(first);
        CHECK(copy.raw_message() != first.raw_message());
        CHECK(copy.payload().Size() == 1 && copy.payload().Data()[0] == 7);
    }
}

static void exchange(unsigned count) {
    int before = fd_count(); double start = now_seconds();
    for (unsigned i = 0; i < count; ++i) {
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        {
            auto a = uipc::connection::from_socket(fds[0], "a");
            auto b = uipc::connection::from_socket(fds[1], "b");
            unsigned char payload[] = {0,255,42};
            uipc::request req(UIPC_VERSION, i, payload, sizeof(payload));
            CHECK(a.send_message(req) == 13);
            auto received = b.read_message();
            CHECK(received.request_id() == i && received.type() == uipc::message_type::REQUEST);
            CHECK(received.payload().Size() == 3 && !memcmp(received.payload().Data(), payload, 3));
            auto request = uipc::request::from_message(received);
            auto response = uipc::response::from_request(request, payload, 3);
            CHECK(b.send_message(response) == 13);
            auto reply = a.read_message(); CHECK(reply.type() == uipc::message_type::RESPONSE && reply.request_id() == i);
        }
    }
    if (before >= 0) CHECK(fd_count() == before);
    printf("cpp: %u exchanges in %.3fs\n", count, now_seconds() - start);
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    if (!strcmp(argv[1], "exchange")) exchange(1);
    else if (!strcmp(argv[1], "stress")) exchange(5000);
    else if (!strcmp(argv[1], "message")) {
        unsigned char data[] = {0,255,1};
        uipc::request req(UIPC_VERSION, 77, data, 3); data[0] = 9;
        CHECK(req.payload().Data()[0] == 0 && req.raw_message()->payload[0] == 0);
        auto response = uipc::response::from_request(req, req.payload().Data(), 3);
        CHECK(response.request_id() == 77 && response.version() == UIPC_VERSION);
        CHECK(response.payload().Size() == 3 && !memcmp(response.payload().Data(), req.payload().Data(), 3));
        auto first = uipc::request::new_request_id(); CHECK(uipc::request::new_request_id() == first + 1);
    } else if (!strcmp(argv[1], "fragmented")) {
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        auto client = uipc::connection::from_socket(fds[0], "peer");
        std::thread writer([&] {
            const unsigned char frame[] = {0,0,0,9,1,2,0,0,0,42,'a',0,'b'};
            for (unsigned char byte : frame) raw_send(fds[1], &byte, 1);
        });
        auto msg = client.read_message();
        CHECK(msg.type() == uipc::message_type::RESPONSE && msg.request_id() == 42);
        CHECK(msg.payload().Size() == 3 && !memcmp(msg.payload().Data(), "a\0b", 3));
        writer.join(); close(fds[1]);
    } else if (!strcmp(argv[1], "raw_read")) {
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        auto client = uipc::connection::from_socket(fds[0], "peer");
        raw_send(fds[1], "a", 1);
        char bytes[3] = {}; CHECK(client.read(bytes, sizeof(bytes)) == 1 && bytes[0] == 'a');
        raw_send(fds[1], "xyz", 3);
        CHECK(client.read_bytes(bytes, 3) == 3 && !memcmp(bytes, "xyz", 3));
        close(fds[1]); CHECK(client.read_bytes(bytes, 3) < 0);
    } else if (!strcmp(argv[1], "large_exchange")) {
        std::vector<unsigned char> data(2 * 1024 * 1024);
        for (size_t i = 0; i < data.size(); ++i) data[i] = (unsigned char)(i * 17);
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        auto client = uipc::connection::from_socket(fds[0], "client");
        auto peer = uipc::connection::from_socket(fds[1], "server");
        std::thread receiver([&] {
            auto msg = peer.read_message();
            CHECK(msg.payload().Size() == data.size() && !memcmp(msg.payload().Data(), data.data(), data.size()));
            CHECK(msg.request_id() == 19);
        });
        uipc::request request(UIPC_VERSION, 19, data.data(), data.size());
        CHECK(client.send_message(request) == (int)data.size() + 10);
        receiver.join();
    } else if (!strcmp(argv[1], "missing_server")) {
        setup_path();
        uipc::connection client(test_path); CHECK(!client.connect());
    } else if (!strcmp(argv[1], "empty")) {
        uipc::request req(UIPC_VERSION, 0, nullptr, 0);
        CHECK(req.length() == 6 && req.payload().Size() == 0);
        auto response = uipc::response::from_request(req, nullptr, 0); CHECK(response.length() == 6);
    } else if (!strcmp(argv[1], "version")) {
        uipc::request req(0, 1, nullptr, 0); CHECK(req.version() == 0);
    } else if (!strcmp(argv[1], "copy_ownership")) {
        check_message_copy<uipc::message>();
    } else if (!strcmp(argv[1], "connection_ownership")) {
        CHECK(!std::is_copy_constructible_v<uipc::connection>);
        CHECK(!std::is_copy_assignable_v<uipc::connection>);
    } else if (!strcmp(argv[1], "server_ownership")) {
        CHECK(!std::is_copy_constructible_v<uipc::server>);
        CHECK(!std::is_copy_assignable_v<uipc::server>);
    } else if (!strcmp(argv[1], "close_idempotent")) {
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        auto c = uipc::connection::from_socket(fds[0], "peer");
        c.close(); c.close();
        int replacement = dup(fds[1]); CHECK(replacement >= 0);
        c.close(); CHECK(fcntl(replacement, F_GETFD) >= 0);
        close(replacement); close(fds[1]);
    } else if (!strcmp(argv[1], "server_lifecycle")) {
        setup_path(); int before = fd_count();
        {
            uipc::server server(test_path);
            CHECK(server.listen());
        }
        CHECK(access(test_path, F_OK) == -1);
        if (before >= 0) CHECK(fd_count() == before);
    } else if (!strcmp(argv[1], "server_exchange")) {
        setup_path();
        uipc::server server(test_path); CHECK(server.listen());
        Connection *raw = connection_allocate(test_path); CHECK(raw && connection_connect(raw));
        {
            auto accepted = server.accept_connection();
            Message *request = request_new((const uint8_t*)"hello", 5); CHECK(request);
            CHECK(connection_send_message(raw, request) == 15);
            auto msg = accepted.read_message();
            CHECK(msg.payload().Size() == 5 && !memcmp(msg.payload().Data(), "hello", 5));
            uipc_free(request);
        }
        release_connection(raw); server.close();
        /* LSan also checks that the wrapper releases its server allocation. */
    } else if (!strcmp(argv[1], "connect")) {
        setup_path(); Server *server = server_create(test_path); CHECK(server && server_listen(server));
        { uipc::connection client(test_path); CHECK(client.connect());
          Connection *accepted = server_accept(server); CHECK(accepted); release_connection(accepted); }
        release_server(server);
    } else if (!strcmp(argv[1], "closed_peer")) {
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        auto client = uipc::connection::from_socket(fds[0], "peer"); close(fds[1]);
        try { auto msg = client.read_message(); CHECK(0); }
        catch (const std::exception&) { /* EOF must be represented safely. */ }
    } else if (!strcmp(argv[1], "packet_interop")) {
        Packet *packet = packet_new(packet_object_new_string("C packet through C++ message"));
        PacketBuffer bytes = packet_serialize(packet);
        uipc::request req(UIPC_VERSION, 12, bytes, packet_buffer_size(bytes));
        packet_buffer_free(bytes); packet_free(packet);
        PacketBuffer received = packet_buffer_from_data(req.payload().Data(), req.payload().Size());
        Packet *parsed = packet_deserialize(received);
        CHECK(!strcmp(packet_object_string(packet_root(parsed)), "C packet through C++ message"));
        packet_free(parsed); packet_buffer_free(received);
    } else CHECK(0);
    return 0;
}
