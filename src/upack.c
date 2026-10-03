#include "uipc/upack.h"
#include "uipc/uipc.h"
#include <stdio.h>
#include <string.h>
#define STB_DS_IMPLEMENTATION
#include "stb_ds.h"
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

struct _PacketObject;

typedef struct {
    char *key;
    struct _PacketObject *value;
} CompoundRecord;

typedef struct {
    struct _PacketObject *items;
    size_t count;
    size_t capacity;
} ArrayObject;

typedef struct _PacketObject {
    PacketObjectType type;

    union {
        CompoundRecord *compound;
        struct _PacketObject **array;
        int integer;
        char *string;
    };
} _PacketObject;

typedef struct {
    PacketObject *root;
} _Packet;

Packet *packet_new(PacketObject *root) {
    _Packet *packet = (_Packet*)malloc(sizeof(_Packet));
    packet->root = root;
    return (Packet*)packet;
}

const PacketObject *packet_root(const Packet *packet) {
    return ((_Packet*)packet)->root;
}

// ------------ Constructing ----------------

static PacketObject *packet_object_new(PacketObjectType type) {
    _PacketObject *object = (_PacketObject*)malloc(sizeof(_PacketObject));
    memset(object, 0, sizeof(*object));
    object->type = type;
    return (PacketObject*)object;
}

PacketObjectType packet_object_type(const PacketObject *object) {
    assert(object != NULL);
    return ((_PacketObject*)object)->type;
}

PacketObject *packet_object_new_integer(int value) {
    _PacketObject *object = (_PacketObject*)packet_object_new(PACKET_OBJECT_INTEGER);
    object->integer = value;
    return (PacketObject*)object;
}

static PacketObject *packet_object_new_sized_string(const char *value, size_t len) {
    _PacketObject *object = (_PacketObject*)packet_object_new(PACKET_OBJECT_STRING);
    object->string = (char*)malloc(sizeof(char) * len + 1);
    assert(object->string != NULL);
    memcpy(object->string, value, sizeof(char) * len);
    return (PacketObject*)object;
}

PacketObject *packet_object_new_string(const char *value) {
    return packet_object_new_sized_string(value, strlen(value));
}

PacketObject *packet_object_new_array(uint32_t capacity) {
    _PacketObject *object = (_PacketObject*)packet_object_new(PACKET_OBJECT_ARRAY);
    object->array = NULL;
    stbds_arrsetcap(object->array, capacity);
    return (PacketObject*)object;
}

PacketObject *packet_object_new_compound() {
    _PacketObject *object = (_PacketObject*)packet_object_new(PACKET_OBJECT_COMPOUND);
    object->compound = NULL;
    return (PacketObject*)object;
}

const char *packet_object_string(const PacketObject *string) {
    assert(packet_object_type(string) == PACKET_OBJECT_STRING);
    return ((_PacketObject*)string)->string;
}

PacketObject *packet_array_insert(PacketObject *restrict array, PacketObject *restrict item) {
    assert(packet_object_type(array) == PACKET_OBJECT_ARRAY);
    assert(item != NULL);
    _PacketObject *object = (_PacketObject*)array;
    return (PacketObject*)stbds_arrput(object->array, (_PacketObject*)item);
}


const PacketObject *packet_array_get(const PacketObject *array, size_t index) {
    assert(packet_object_type(array) == PACKET_OBJECT_ARRAY);
    return (PacketObject*)((_PacketObject*)array)->array[index];
}

size_t packet_array_len(const PacketObject *array) {
    assert(packet_object_type(array) == PACKET_OBJECT_ARRAY);
    _PacketObject *object = (_PacketObject*)array;
    return stbds_arrlenu(object->array);
}

PacketObject *packet_compound_insert(PacketObject *restrict compound, const char* key, PacketObject *restrict value) {
    assert(packet_object_type(compound) == PACKET_OBJECT_COMPOUND);
    _PacketObject *object = (_PacketObject*)compound;
    return (PacketObject*)stbds_shput(object->compound, key, (_PacketObject*)value);
}

const PacketObject *packet_compound_get(const PacketObject *compound, const char* key) {
    assert(packet_object_type(compound) == PACKET_OBJECT_COMPOUND);
    _PacketObject *object = (_PacketObject*)compound;
    ptrdiff_t index = stbds_shgeti(object->compound, key);
    if (index == -1) return NULL;
    return (PacketObject*)object->compound[index].value;
}

bool packet_compound_key_exists(const PacketObject *compound, const char* key) {
    assert(packet_object_type(compound) == PACKET_OBJECT_COMPOUND);
    _PacketObject *object = (_PacketObject*)compound;
    ptrdiff_t index = stbds_shgeti(object->compound, key);
    return index != -1;
}

// -------- Serializing ----------

static void packet_serialize_object(PacketBuffer *buffer, PacketObject *object);

static size_t packet_varint_serialize(PacketBuffer *buffer, uint32_t value) {
    size_t count = 0;

    while (value >= 0x80) {
        stbds_arrput(*buffer, (uint8_t)((value & 0x7F) | 0x80));
        value >>= 7;
        count++;
    }

    stbds_arrput(*buffer, (uint8_t)value);
    count++;

    return count;
}

static void packet_buffer_insert_sized_string(PacketBuffer *buffer, const char* string) {
    size_t n = strlen(string);
    size_t s = packet_varint_serialize(buffer, n);
    PacketBuffer cursor = stbds_arraddnptr(*buffer, n);
    memcpy(cursor, string, n);
}

static void packet_integer_serialize(PacketBuffer *buffer, _PacketObject *integer) {
    assert(packet_object_type((PacketObject*)integer) == PACKET_OBJECT_INTEGER);
    packet_varint_serialize(buffer, integer->integer);
}

static void packet_string_serialize(PacketBuffer *buffer, _PacketObject *string) {
    assert(packet_object_type((PacketObject*)string) == PACKET_OBJECT_STRING);
    packet_buffer_insert_sized_string(buffer, string->string);
}

static void packet_array_serialize(PacketBuffer *buffer, _PacketObject *array) {
    assert(packet_object_type((PacketObject*)array) == PACKET_OBJECT_ARRAY);
    PacketBuffer arrayBuffer = NULL;
    for (size_t i = 0; i < stbds_arrlenu(array->array); ++i) {
        packet_serialize_object(&arrayBuffer, (PacketObject*)array->array[i]);
    }
    size_t arrSize = stbds_arrlenu(arrayBuffer);
    packet_varint_serialize(buffer, arrSize);
    PacketBuffer cursor = stbds_arraddnptr(*buffer, arrSize);
    memcpy(cursor, arrayBuffer, arrSize);
    stbds_arrfree(arrayBuffer);
}

static void packet_compound_serialize(PacketBuffer *buffer, _PacketObject *compound) {
    assert(packet_object_type((PacketObject*)compound) == PACKET_OBJECT_COMPOUND);
    PacketBuffer compoundBuffer = NULL;
    for (size_t i = 0; i < stbds_shlenu(compound->compound); ++i) {
        CompoundRecord record = compound->compound[i];
        packet_buffer_insert_sized_string(&compoundBuffer, record.key);
        packet_serialize_object(&compoundBuffer, (PacketObject*)record.value);
    }
    size_t compoundSize = stbds_arrlenu(compoundBuffer);
    packet_varint_serialize(buffer, compoundSize);
    PacketBuffer cursor = stbds_arraddnptr(*buffer, compoundSize);
    memcpy(cursor, compoundBuffer, compoundSize);
    stbds_arrfree(compoundBuffer);
}

static void packet_serialize_object(PacketBuffer *buffer, PacketObject *object) {
    PacketObjectType type = packet_object_type(object);
    stbds_arrput(*buffer, type);
    switch (type) {
        case PACKET_OBJECT_INTEGER: packet_integer_serialize(buffer, (_PacketObject*)object); break;
        case PACKET_OBJECT_STRING: packet_string_serialize(buffer, (_PacketObject*)object); break;
        case PACKET_OBJECT_ARRAY: packet_array_serialize(buffer, (_PacketObject*)object); break;
        case PACKET_OBJECT_COMPOUND: packet_compound_serialize(buffer, (_PacketObject*)object); break;
        default: assert(false && "unsupported packet object type"); break;
    }
}

PacketBuffer packet_serialize(Packet *packet) {
    _Packet *pk = (_Packet*)packet;
    PacketBuffer buffer = NULL;
    packet_serialize_object(&buffer, pk->root);
    return buffer;
}

PacketBuffer packet_buffer_from_data(const uint8_t *data, size_t len) {
    PacketBuffer buffer = NULL;
    stbds_arrsetlen(buffer, len);
    memcpy(buffer, data, sizeof(uint8_t) * len);
    return buffer;
}

size_t packet_buffer_size(const PacketBuffer buffer) {
    return stbds_arrlenu(buffer);
}

PacketBuffer packet_buffer_end(const PacketBuffer buffer) {
    return buffer + stbds_arrlen(buffer);
}

static void packet_object_free(PacketObject *object);

static void packet_string_object_free(PacketObject *string) {
    uipc_free(((_PacketObject*)string)->string);
}

static void packet_array_object_free(PacketObject *object) {
    assert(packet_object_type(object) == PACKET_OBJECT_ARRAY);
    _PacketObject **array = ((_PacketObject*)object)->array;
    for (size_t i = 0; i < stbds_arrlenu(array); ++i) {
        packet_object_free((PacketObject*)array[i]);
    }
    stbds_arrfree(array);
    ((_PacketObject*)object)->array = NULL;
}

static void packet_compound_object_free(PacketObject *object) {
    assert(packet_object_type(object) == PACKET_OBJECT_COMPOUND);
    CompoundRecord *compound = ((_PacketObject*)object)->compound;
    for (size_t i = 0; i < stbds_shlenu(compound); ++i) {
        packet_object_free((PacketObject*)(compound[i].value));
    }
    stbds_shfree(compound);
    ((_PacketObject*)object)->compound = NULL;
}

void packet_buffer_free(PacketBuffer buffer) {
    stbds_arrfree(buffer);
}

static void packet_object_free(PacketObject *object) {
    assert(object != NULL);
    if (packet_object_type(object) == PACKET_OBJECT_ARRAY) packet_array_object_free(object);
    if (packet_object_type(object) == PACKET_OBJECT_COMPOUND) packet_compound_object_free(object);
    if (packet_object_type(object) == PACKET_OBJECT_STRING) packet_string_object_free(object);
    uipc_free((_PacketObject*)object);
}

void packet_free(Packet *packet) {
    packet_object_free(((_Packet*)packet)->root);
    uipc_free(packet);
}

// ------------- Deserialize ----------------

static PacketObject *packet_object_deserialize(PacketBuffer *cursor);

static uint32_t packet_varint_deserialize(PacketBuffer *buffer, uint32_t *result) {
    uint32_t value = 0;
    unsigned shift = 0;
    for (unsigned i = 0; i < 5; ++i) {
        uint8_t byte = *(*buffer)++;
        value |= (uint32_t)(byte & 0x7F) << shift;
        if ((byte & 0x80) == 0) {
            *result = value;
            return true;
        }

        shift += 7;
    }

    return false;
}

static PacketObject *packet_integer_deserialize(PacketBuffer *cursor) {
    uint32_t value = 0;
    assert(packet_varint_deserialize(cursor, &value));
    return packet_object_new_integer(value);
}

static char *packet_buffer_extract_sized_string(PacketBuffer *cursor) {
    uint32_t len = 0;
    assert(packet_varint_deserialize(cursor, &len));
    char *buf = (char*)malloc(sizeof(char) * (len + 1));
    memcpy(buf, *cursor, sizeof(char) * (len));
    buf[len] = '\0';
    *cursor += len;
    return buf;
}

static PacketObject *packet_string_deserialize(PacketBuffer *cursor) {
    return packet_object_new_string(packet_buffer_extract_sized_string(cursor));
}

static PacketObject *packet_array_deserialize(PacketBuffer *cursor) {
    PacketObject *array = packet_object_new_array(128);
    uint32_t arrSize = 0;
    assert(packet_varint_deserialize(cursor, &arrSize));
    PacketBuffer start = *cursor;
    while (*cursor - start < arrSize) {
        packet_array_insert(array, packet_object_deserialize(cursor));
    }
    return array;
}

static PacketObject *packet_compound_deserialize(PacketBuffer *cursor) {
    PacketObject *compound = packet_object_new_compound();
    uint32_t compoundSize = 0;
    assert(packet_varint_deserialize(cursor, &compoundSize));
    PacketBuffer start = *cursor;
    while (*cursor - start < compoundSize) {
        char *key = packet_buffer_extract_sized_string(cursor);
        PacketObject *value = packet_object_deserialize(cursor);
        packet_compound_insert(compound, key, value);
    }
    return compound;
}

static PacketObject *packet_object_deserialize(PacketBuffer *cursor) {
    PacketObjectType type = (PacketObjectType)**cursor;
    PacketBuffer original = *cursor;
    *cursor += 1;
    switch (type) {
        case PACKET_OBJECT_INTEGER: return packet_integer_deserialize(cursor); break;
        case PACKET_OBJECT_STRING: return packet_string_deserialize(cursor); break;
        case PACKET_OBJECT_ARRAY: return packet_array_deserialize(cursor); break;
        case PACKET_OBJECT_COMPOUND: return packet_compound_deserialize(cursor); break;
        default: assert(false && "unsupported packet object type"); break;
    }
    assert(false && "UNREACHABLE");
    return NULL;
}

Packet *packet_deserialize(const PacketBuffer buffer) {
    PacketBuffer cursor = buffer;
    PacketObject *root = packet_object_deserialize(&cursor);
    assert(cursor == packet_buffer_end(buffer));
    return packet_new(root);
}
