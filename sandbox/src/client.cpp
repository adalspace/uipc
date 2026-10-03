#include "uipc/uipc.h"
#include <iostream>
#include <print>
#include <uipc/uipc.hpp>
#include <uipc/upack.h>

int main(void) {
    const char* pathname = "/tmp/73n59c61m58b26n.socket";

    int counter = 1;
    std::string message;
    while (true) {
        std::cin >> message;

        if (message == "exit") {
            break;
        }

        uipc::connection conn(pathname);
        assert(conn.connect());

        PacketObject *compound = packet_object_new_compound();
        packet_compound_insert(compound, "message", packet_object_new_string(message.c_str()));
        Packet *packet = packet_new(compound);
        PacketBuffer buffer = packet_serialize(packet);

        uipc::request req(UIPC_VERSION, counter++, buffer, packet_buffer_size(buffer));
        conn.send_message(req);
        std::println("sent '{}' to the server", message);

        uipc::message res = conn.read_message();
        std::println("received {} bytes from server", res.length());

        conn.close();
    }

    return 0;
}
