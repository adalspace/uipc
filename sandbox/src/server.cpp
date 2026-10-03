#include <uipc/uipc.h>
#include <iostream>
#include <string_view>

int main(int argc, char **argv) {
    const char *pathname = argc > 1 ? argv[1] : "/tmp/uipc.socket";
    bool once = argc > 2 && std::string_view(argv[2]) == "--once";
    Server *server = server_create(pathname);
    if (!server) return 1;
    if (!server_listen(server)) {
        server_close(server);
        uipc_free(server);
        return 1;
    }
    std::cout << "listening on " << server_address(server) << std::endl;
    int result = 0;
    do {
        Connection *conn = server_accept(server);
        if (!conn) { result = 1; break; }
        Message *request = connection_recv_message(conn);
        Message *response = request && request->type == MSG_REQUEST
            ? response_new(request, request->payload, request->length - 6) : nullptr;
        if (!response || connection_send_message(conn, response) < 0) {
            std::cerr << "Failed to handle request\n";
            if (once) result = 1;
        }
        uipc_free(request);
        uipc_free(response);
        connection_close(conn);
        uipc_free(conn);
    } while (!once);
    server_close(server);
    uipc_free(server);
    return result;
}
