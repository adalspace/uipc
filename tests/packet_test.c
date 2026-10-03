#include "common.h"
#include <uipc/upack.h>

static void roundtrip(PacketObject *root) {
    Packet *p = packet_new(root); CHECK(p);
    PacketBuffer first = packet_serialize(p); CHECK(first);
    PacketBuffer copied = packet_buffer_from_data(first, packet_buffer_size(first)); CHECK(copied);
    CHECK(!memcmp(first, copied, packet_buffer_size(first)));
    Packet *parsed = packet_deserialize(copied); CHECK(parsed);
    PacketBuffer second = packet_serialize(parsed); CHECK(second);
    CHECK(packet_buffer_size(first) == packet_buffer_size(second));
    CHECK(!memcmp(first, second, packet_buffer_size(first)));
    packet_buffer_free(second); packet_free(parsed); packet_buffer_free(copied);
    packet_buffer_free(first); packet_free(p);
}
static void golden_integer(int value, const unsigned char *expected, size_t size) {
    Packet *p = packet_new(packet_object_new_integer(value)); CHECK(p);
    PacketBuffer bytes = packet_serialize(p);
    CHECK(packet_buffer_size(bytes) == size && !memcmp(bytes, expected, size));
    PacketBuffer input = packet_buffer_from_data(expected, size);
    Packet *parsed = packet_deserialize(input); CHECK(parsed);
    CHECK(packet_object_type(packet_root(parsed)) == PACKET_OBJECT_INTEGER);
    PacketBuffer encoded = packet_serialize(parsed);
    CHECK(packet_buffer_size(encoded) == size && !memcmp(encoded, expected, size));
    packet_buffer_free(encoded); packet_free(parsed); packet_buffer_free(input);
    packet_buffer_free(bytes); packet_free(p);
}
static void string_case(size_t size) {
    char *value = malloc(size + 1); CHECK(value);
    for (size_t i = 0; i < size; ++i) value[i] = (char)('!' + i % 90);
    value[size] = 0;
    Packet *p = packet_new(packet_object_new_string(value)); CHECK(p);
    CHECK(!strcmp(packet_object_string(packet_root(p)), value));
    PacketBuffer bytes = packet_serialize(p); CHECK(bytes);
    Packet *parsed = packet_deserialize(bytes); CHECK(parsed);
    CHECK(!strcmp(packet_object_string(packet_root(parsed)), value));
    packet_free(parsed); packet_buffer_free(bytes); packet_free(p); free(value);
}
static void malformed(const unsigned char *data, size_t size) {
    PacketBuffer bytes = packet_buffer_from_data(data, size);
    Packet *parsed = packet_deserialize(bytes);
    /* Desired contract: untrusted bytes are rejected without aborts, overreads or leaks. */
    CHECK(parsed == NULL);
    packet_buffer_free(bytes);
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    if (!strcmp(argv[1], "integers")) {
        const unsigned char zero[] = {3,0}, one[] = {3,1}, a[] = {3,127}, b[] = {3,128,1};
        const unsigned char c[] = {3,255,127}, d[] = {3,128,128,1};
        const unsigned char max[] = {3,255,255,255,255,7}, min[] = {3,128,128,128,128,8};
        const unsigned char neg[] = {3,255,255,255,255,15};
        golden_integer(0, zero, sizeof(zero)); golden_integer(1, one, sizeof(one));
        golden_integer(127, a, sizeof(a)); golden_integer(128, b, sizeof(b));
        golden_integer(16383, c, sizeof(c)); golden_integer(16384, d, sizeof(d));
        golden_integer(INT_MAX, max, sizeof(max)); golden_integer(INT_MIN, min, sizeof(min));
        golden_integer(-1, neg, sizeof(neg));
    } else if (!strcmp(argv[1], "golden_tree")) {
        const unsigned char expected[] = {1,6,1,'k',4,2,'h','i'};
        PacketObject *root = packet_object_new_compound();
        packet_compound_insert(root, "k", packet_object_new_string("hi"));
        Packet *p = packet_new(root); PacketBuffer bytes = packet_serialize(p);
        CHECK(packet_buffer_size(bytes) == sizeof(expected) && !memcmp(bytes, expected, sizeof(expected)));
        PacketBuffer input = packet_buffer_from_data(expected, sizeof(expected));
        Packet *parsed = packet_deserialize(input); CHECK(parsed);
        CHECK(!strcmp(packet_object_string(packet_compound_get(packet_root(parsed), "k")), "hi"));
        packet_free(parsed); packet_buffer_free(input); packet_buffer_free(bytes); packet_free(p);
    } else if (!strcmp(argv[1], "seeded")) {
        unsigned state = 0x12345678;
        for (int n = 0; n < 250; ++n) {
            PacketObject *array = packet_object_new_array(0);
            state = state * 1664525u + 1013904223u; unsigned count = state % 64;
            for (unsigned i = 0; i < count; ++i) {
                state = state * 1664525u + 1013904223u;
                if (state & 1) packet_array_insert(array, packet_object_new_integer((int)(state & INT_MAX)));
                else packet_array_insert(array, packet_object_new_string("seeded string"));
            }
            CHECK(packet_array_len(array) == count); roundtrip(array);
        }
    } else if (!strcmp(argv[1], "strings")) {
        const size_t sizes[] = {0,1,127,128,16383,16384,65536};
        for (size_t i = 0; i < sizeof(sizes)/sizeof(*sizes); ++i) string_case(sizes[i]);
    } else if (!strcmp(argv[1], "empty")) {
        roundtrip(packet_object_new_array(0)); roundtrip(packet_object_new_compound());
    } else if (!strcmp(argv[1], "nested")) {
        PacketObject *root = packet_object_new_compound();
        PacketObject *items = packet_object_new_array(0);
        for (int i = 0; i < 300; ++i) packet_array_insert(items, packet_object_new_integer(i));
        CHECK(packet_array_len(items) == 300);
        CHECK(packet_object_type(packet_array_get(items, 299)) == PACKET_OBJECT_INTEGER);
        packet_array_insert(items, packet_object_new_string("hello"));
        packet_compound_insert(root, "items", items);
        CHECK(packet_compound_key_exists(root, "items"));
        CHECK(packet_compound_get(root, "items") == items);
        CHECK(!packet_compound_key_exists(root, "missing") && !packet_compound_get(root, "missing"));
        roundtrip(root);
    } else if (!strcmp(argv[1], "key_ownership")) {
        PacketObject *root = packet_object_new_compound();
        char key[] = "original";
        packet_compound_insert(root, key, packet_object_new_string("owned"));
        memset(key, 'x', 8);
        CHECK(packet_compound_key_exists(root, "original")); roundtrip(root);
    } else if (!strcmp(argv[1], "replacement")) {
        PacketObject *root = packet_object_new_compound();
        packet_compound_insert(root, "same", packet_object_new_string("old"));
        packet_compound_insert(root, "same", packet_object_new_string("new"));
        CHECK(!strcmp(packet_object_string(packet_compound_get(root, "same")), "new"));
        roundtrip(root); /* Sanitizers must catch the replaced child's leaked allocation. */
    } else if (!strcmp(argv[1], "depth")) {
        PacketObject *root = packet_object_new_string("leaf");
        for (int i = 0; i < 64; ++i) {
            PacketObject *parent = packet_object_new_array(1); packet_array_insert(parent, root); root = parent;
        }
        roundtrip(root);
    } else if (!strcmp(argv[1], "stress")) {
        double start = now_seconds();
        for (int iteration = 0; iteration < 5000; ++iteration) {
            PacketObject *root = packet_object_new_compound();
            for (int i = 0; i < 32; ++i) {
                char key[32]; snprintf(key, sizeof(key), "key_%d", i);
                packet_compound_insert(root, key, packet_object_new_string("repeat allocation and recursive cleanup"));
            }
            roundtrip(root);
        }
        string_case(4 * 1024 * 1024);
        printf("packet: 5000 trees + 4 MiB string in %.3fs\n", now_seconds() - start);
    } else if (!strcmp(argv[1], "malformed_type")) {
        unsigned char data[] = {255}; malformed(data, sizeof(data));
    } else if (!strcmp(argv[1], "malformed_string")) {
        unsigned char data[] = {4,127,'x'}; malformed(data, sizeof(data));
    } else if (!strcmp(argv[1], "malformed_varint")) {
        unsigned char data[] = {3,128,128,128,128,128}; malformed(data, sizeof(data));
    } else if (!strcmp(argv[1], "malformed_container")) {
        unsigned char data[] = {2,100,3,1}; malformed(data, sizeof(data));
    } else if (!strcmp(argv[1], "trailing")) {
        unsigned char data[] = {3,1,0}; malformed(data, sizeof(data));
    } else if (!strcmp(argv[1], "truncated")) {
        unsigned char data[] = {3}; malformed(data, sizeof(data));
    } else CHECK(0);
    return 0;
}
