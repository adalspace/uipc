#include <uipc/uipc.h>
#include <uipc/upack.h>
#include <iostream>
#include <string>
#include <cstring>

int main(int argc, char **argv) {
    const char *pathname = argc > 1 ? argv[1] : "/tmp/uipc.socket";
    std::string text;
    while (std::getline(std::cin, text) && text != "exit") {
        Connection *conn = connection_allocate(pathname);
        if (!conn || !connection_connect(conn)) {
            connection_close(conn);
            uipc_free(conn);
            return 1;
        }
        PacketObject *root = packet_object_new_compound();
        packet_compound_insert(root, "message", packet_object_new_string(text.c_str()));
        Packet *packet = packet_new(root);
        PacketBuffer bytes = packet_serialize(packet);
        Message *request = request_new(bytes, packet_buffer_size(bytes));
        packet_buffer_free(bytes);
        packet_free(packet);
        int sent = request ? connection_send_message(conn, request) : -1;
        Message *response = sent >= 0 ? connection_recv_message(conn) : nullptr;
        bool valid = response && response->type == MSG_RESPONSE &&
            response->request_id == request->request_id && response->length == request->length;
        if (valid) {
            size_t size = response->length - 6;
            valid = std::memcmp(request->payload, response->payload, size) == 0;
            if (valid) std::cout << text << '\n';
        } else {
            std::cerr << "Failed to receive matching response\n";
        }
        uipc_free(request);
        uipc_free(response);
        connection_close(conn);
        uipc_free(conn);
        if (!valid) return 1;
    }
    return 0;
}
