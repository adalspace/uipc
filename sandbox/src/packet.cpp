#include <uipc/upack.h>
#include <cstring>
#include <iostream>

#define CHECK(expr) do { if (!(expr)) { fprintf(stderr, "%s:%d: CHECK(%s) failed (errno=%d)\n", __FILE__, __LINE__, #expr, errno); exit(1); } } while (0)

static void malformed(const unsigned char *data, size_t size) {
    PacketBuffer bytes = packet_buffer_from_data(data, size);
    Packet *parsed = packet_deserialize(bytes);
    /* Desired contract: untrusted bytes are rejected without aborts, overreads or leaks. */
    CHECK(parsed == NULL);
    packet_buffer_free(bytes);
}

int main() {
    unsigned char data[] = {2,100,3,1}; malformed(data, sizeof(data));
    return 0;
    const char *values[] = {"", "hello", "a longer string with spaces"};
    for (const char *value : values) {
        PacketObject *root = packet_object_new_compound();
        PacketObject *array = packet_object_new_array(2);
        packet_array_insert(array, packet_object_new_string(value));
        packet_array_insert(array, packet_object_new_integer(-123));
        packet_compound_insert(root, "items", array);
        Packet *packet = packet_new(root);
        PacketBuffer bytes = packet_serialize(packet);
        Packet *parsed = packet_deserialize(bytes);
        const PacketObject *items = packet_compound_get(packet_root(parsed), "items");
        bool valid = items && packet_array_len(items) == 2 &&
            std::strcmp(packet_object_string(packet_array_get(items, 0)), value) == 0;
        PacketBuffer encoded = packet_serialize(parsed);
        valid = valid && packet_buffer_size(bytes) == packet_buffer_size(encoded) &&
            std::memcmp(bytes, encoded, packet_buffer_size(bytes)) == 0;
        packet_buffer_free(encoded);
        packet_buffer_free(bytes);
        packet_free(parsed);
        packet_free(packet);
        if (!valid) { std::cerr << "Packet round trip failed\n"; return 1; }
    }
    std::cout << "Packet round trips passed\n";
    return 0;
}
