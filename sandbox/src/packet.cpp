#include <cstring>
#include <stdio.h>
#include <sys/random.h>
#include <assert.h>
#include <fcntl.h>
#include <unistd.h>
#include "uipc/upack.h"

char *generate_random_string(size_t len) {
    char *buf = (char*)malloc(sizeof(char) * len + 1);
    const char charset[] =
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "0123456789";

    unsigned char random_bytes[len];

    int fd = open("/dev/urandom", O_RDONLY);

    size_t total = 0;

    while (total < sizeof(random_bytes)) {
        ssize_t n = read(
            fd,
            random_bytes + total,
            sizeof(random_bytes) - total
        );

        if (n <= 0) {
            // handle error
            break;
        }

        total += (size_t)n;
    }

    close(fd);

    for (size_t i = 0; i < len; ++i) {
        buf[i] = charset[random_bytes[i] % (sizeof(charset) - 1)];
    }

    buf[len] = '\0';
    return buf;
}

int main(void) {
    // Packet *packet = packet_new(packet_object_new_string("Hello World!"));
    // PacketObject *arr = packet_object_new_array(2);
    // packet_array_insert(arr, packet_object_new_string("Hello, World!"));
    // packet_array_insert(arr, packet_object_new_integer(~0));
    PacketObject *compound = packet_object_new_compound();
    packet_compound_insert(compound, "email", packet_object_new_string("adal.amir07@gmail.com"));
    packet_compound_insert(compound, "age", packet_object_new_integer(21));
    PacketObject *skills = packet_object_new_array(3);
    packet_array_insert(skills, packet_object_new_string("volleyball"));
    packet_array_insert(skills, packet_object_new_string("coding"));
    packet_array_insert(skills, packet_object_new_string("swimming"));
    packet_compound_insert(compound, "skills", skills);
    packet_compound_insert(compound, "password", packet_object_new_string("*********"));
    Packet *packet = packet_new(compound);
    PacketBuffer buffer = packet_serialize(packet);
    // Packet *packet = packet_new(compound);
    // PacketBuffer buffer = packet_serialize(packet);

    ssize_t bufferSize = packet_buffer_size(buffer);
    printf("Packet buffer size: %zu\n", bufferSize);

    int out = open("output.bin", O_CREAT | O_WRONLY, 0644);
    write(out, buffer, bufferSize);
    fsync(out);
    close(out);

    Packet *parsed = packet_deserialize(buffer);
    buffer = packet_serialize(packet);
    bufferSize = packet_buffer_size(buffer);
    printf("Packet buffer size: %zu\n", bufferSize);

    out = open("output2.bin", O_CREAT | O_WRONLY, 0644);
    write(out, buffer, bufferSize);
    fsync(out);
    close(out);

    return 0;
}
