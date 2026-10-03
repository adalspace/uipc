#include <cstdio>
#include <uipc/uipc.hpp>
#include <uipc/upack.h>
#include <print>

int main() {
    uipc::server server("/tmp/73n59c61m58b26n.socket");
    std::println("listening on {}", server.address().c_str());

    server.listen();

    while (true) {
        uipc::connection conn = server.accept_connection();

        uipc::request req = uipc::request::from_message(conn.read_message());

        if (req.payload().Size() > 0) {
            PacketBuffer buffer = packet_buffer_from_data(req.payload().Data(), req.payload().Size());
            Packet *packet = packet_deserialize(buffer);

            const PacketObject *root = packet_root(packet);
            if (packet_object_type(root) == PACKET_OBJECT_COMPOUND) {
                if (packet_compound_key_exists(root, "message")) {
                    const char *message = packet_object_string(packet_compound_get(root, "message"));
                    std::println("message from user: {}", message);
                }
            }

            packet_free(packet);
            packet_buffer_free(buffer);

            uipc::response res = uipc::response::from_request(req, req.payload().Data(), req.payload().Size());
            conn.send_message(res);
            printf("sent message back to the client\n");
        } else {
            printf("received empty message from the client\n");
        }

        conn.close();
    }

    printf("Stopping server\n");
    server.close();

    return 0;
}
