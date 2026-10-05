#include <uipc/uipc.h>
#include <iostream>

Message *request_handler(Message *request) {
    size_t payload_size = 0;
    const uint8_t *payload = message_payload(request, &payload_size);
    return response_new(request, payload, payload_size);
}

int main(int argc, char **argv) {
    const char *pathname = argc > 1 ? argv[1] : "/tmp/uipc.socket";
    Server *server = server_create(pathname);
    if (!server) return 1;
    std::cout << "listening on " << server_address(server) << std::endl;
    server_register_handler(server, request_handler);
    if (!server_listen(server)) {
        server_close(server);
        uipc_free(server);
        return 1;
    }
    server_close(server);
    uipc_free(server);
    return 0;
}
