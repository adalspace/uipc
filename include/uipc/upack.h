#pragma once
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t *PacketBuffer;

typedef struct Packet Packet;
typedef struct PacketObject PacketObject;

typedef uint8_t PacketObjectType;
#define PACKET_OBJECT_COMPOUND 1
#define PACKET_OBJECT_ARRAY 2
#define PACKET_OBJECT_INTEGER 3
#define PACKET_OBJECT_STRING 4

Packet *packet_new(PacketObject *root);
const PacketObject *packet_root(const Packet *packet);

PacketObjectType packet_object_type(const PacketObject *object);
PacketObject *packet_object_new_integer(int value);
PacketObject *packet_object_new_string(const char *value);
PacketObject *packet_object_new_array(uint32_t capacity);
PacketObject *packet_object_new_compound(void);

const char *packet_object_string(const PacketObject *string);

PacketObject *packet_array_insert(PacketObject *array, PacketObject *item);
const PacketObject *packet_array_get(const PacketObject *array, size_t index);
size_t packet_array_len(const PacketObject *array);
PacketObject *packet_compound_insert(PacketObject *compound, const char* key, PacketObject *value);
const PacketObject *packet_compound_get(const PacketObject *compound, const char* key);
bool packet_compound_key_exists(const PacketObject *compound, const char* key);

void packet_free(Packet *packet);
void packet_buffer_free(PacketBuffer buffer);

PacketBuffer packet_serialize(Packet *packet);
Packet *packet_deserialize(const PacketBuffer buffer);

PacketBuffer packet_buffer_from_data(const uint8_t *data, size_t len);
size_t packet_buffer_size(const PacketBuffer buffer);
PacketBuffer packet_buffer_end(const PacketBuffer buffer);

#ifdef __cplusplus
}
#endif
