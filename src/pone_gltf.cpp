#include "pone_gltf.h"

#include "pone_arena.h"
#include "pone_assert.h"
#include "pone_json.h"
#include "pone_memory.h"
#include "pone_string.h"

static void pone_gltf_parse_header(void *data, PoneGltfHeader *header) {
    u32 *data_u32 = (u32 *)data;

    u32 version = *(++data_u32);
    u32 length = *(++data_u32);

    header->version = version;
    header->length = length;
}

static void pone_gltf_parse_chunk(void *data, PoneGltfChunk *chunk) {
    u32 *data_u32 = (u32 *)data;
    u32 length = *data_u32++;
    u8 *type_ascii = (u8 *)data_u32++;
    PoneGltfChunkType type;
    if (pone_string_eq((PoneString){.buf = (u8 *)"JSON", .len = 4},
                       (PoneString){.buf = type_ascii, .len = 4})) {
        type = PONE_GLTF_CHUNK_TYPE_JSON;
    } else {
        type = PONE_GLTF_CHUNK_TYPE_BIN;
    }
    u8 *d = (u8 *)data_u32;

    chunk->length = length;
    chunk->type = type;
    chunk->data = d;
}

static void pone_gltf_parse_accessor(PoneJsonObject *object, Arena *arena,
                                     PoneGltfAccessor *accessor) {
    accessor->buffer_view = 0;
    accessor->byte_offset = 0;
    accessor->component_type = PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNKNOWN;
    accessor->normalized = 0;
    accessor->count = 0;
    accessor->max = 0;
    accessor->min = 0;
    accessor->name = 0;

    for (PoneJsonPair *pair = object->pairs; pair; pair = pair->next) {
        PoneString name = pair->name;
        PoneJsonValue *value = pair->value;
        if (pone_string_eq(
                name, (PoneString){.buf = (u8 *)"bufferView", .len = 10})) {
            accessor->buffer_view = (u32 *)arena_alloc(arena, sizeof(u32));
            *accessor->buffer_view = (u32)value->number;
        } else if (pone_string_eq(name, (PoneString){.buf = (u8 *)"byteOffset",
                                                     .len = 10})) {
            accessor->byte_offset = (u32)value->number;
        } else if (pone_string_eq(
                       name,
                       (PoneString){.buf = (u8 *)"componentType", .len = 13})) {
            accessor->component_type =
                (PoneGltfAccessorComponentType)(u32)value->number;
        } else if (pone_string_eq(name, (PoneString){.buf = (u8 *)"normalized",
                                                     .len = 10})) {
            if (value->type == PONE_JSON_TYPE_TRUE) {
                accessor->normalized = 1;
            } else {
                accessor->normalized = 0;
            }
        } else if (pone_string_eq(
                       name, (PoneString){.buf = (u8 *)"count", .len = 5})) {
            accessor->count = (usize)value->number;
        } else if (pone_string_eq(
                       name, (PoneString){.buf = (u8 *)"type", .len = 4})) {
            if (pone_string_eq(value->string,
                               (PoneString){.buf = (u8 *)"SCALAR", .len = 6})) {
                accessor->type = PONE_GLTF_ACCESSOR_TYPE_SCALAR;
            } else if (pone_string_eq(
                           value->string,
                           (PoneString){.buf = (u8 *)"VEC2", .len = 4})) {
                accessor->type = PONE_GLTF_ACCESSOR_TYPE_VEC2;
            } else if (pone_string_eq(
                           value->string,
                           (PoneString){.buf = (u8 *)"VEC3", .len = 4})) {
                accessor->type = PONE_GLTF_ACCESSOR_TYPE_VEC3;
            } else if (pone_string_eq(
                           value->string,
                           (PoneString){.buf = (u8 *)"VEC4", .len = 4})) {
                accessor->type = PONE_GLTF_ACCESSOR_TYPE_VEC4;
            } else if (pone_string_eq(
                           value->string,
                           (PoneString){.buf = (u8 *)"MAT2", .len = 4})) {
                accessor->type = PONE_GLTF_ACCESSOR_TYPE_MAT2;
            } else if (pone_string_eq(
                           value->string,
                           (PoneString){.buf = (u8 *)"MAT3", .len = 4})) {
                accessor->type = PONE_GLTF_ACCESSOR_TYPE_MAT3;
            } else if (pone_string_eq(
                           value->string,
                           (PoneString){.buf = (u8 *)"MAT4", .len = 4})) {
                accessor->type = PONE_GLTF_ACCESSOR_TYPE_MAT4;
            } else {
                PONE_ASSERT(0 && "Invalid accessor type");
            }
        } else if (pone_string_eq(name,
                                  (PoneString){.buf = (u8 *)"max", .len = 3})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_ARRAY);
            PoneJsonArray *array = &value->array;

            switch (accessor->component_type) {
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_BYTE: {
                accessor->max = arena_alloc_array(arena, array->count, i8);
            } break;
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_BYTE: {
                accessor->max = arena_alloc_array(arena, array->count, u8);
            } break;

            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_SHORT: {
                accessor->max = arena_alloc_array(arena, array->count, i16);
            } break;

            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_SHORT: {
                accessor->max = arena_alloc_array(arena, array->count, u16);
            } break;
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_INT: {
                accessor->max = arena_alloc_array(arena, array->count, u32);
            } break;
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT: {
                accessor->max = arena_alloc_array(arena, array->count, f32);
            } break;
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNKNOWN: {
                accessor->max = arena_alloc_array(arena, array->count, f32);
            } break;
            }

            PoneJsonArrayValue *array_value = array->values;
            for (usize max_index = 0; max_index < array->count; ++max_index) {
                PONE_ASSERT(array_value);

                switch (accessor->component_type) {
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_BYTE: {
                    ((i8 *)accessor->max)[max_index] =
                        (i8)array_value->value->number;
                } break;
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_BYTE: {
                    ((u8 *)accessor->max)[max_index] =
                        (u8)array_value->value->number;
                } break;

                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_SHORT: {
                    ((i16 *)accessor->max)[max_index] =
                        (i16)array_value->value->number;
                } break;

                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_SHORT: {
                    ((u16 *)accessor->max)[max_index] =
                        (u16)array_value->value->number;
                } break;
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_INT: {
                    ((u32 *)accessor->max)[max_index] =
                        (u32)array_value->value->number;
                } break;
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT: {
                    ((f32 *)accessor->max)[max_index] =
                        (f32)array_value->value->number;
                } break;
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNKNOWN: {
                    ((f32 *)accessor->max)[max_index] =
                        (f32)array_value->value->number;
                } break;
                }

                array_value = array_value->next;
            }
        } else if (pone_string_eq(name,
                                  (PoneString){.buf = (u8 *)"min", .len = 3})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_ARRAY);
            PoneJsonArray *array = &value->array;

            switch (accessor->component_type) {
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_BYTE: {
                accessor->min = arena_alloc_array(arena, array->count, i8);
            } break;
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_BYTE: {
                accessor->min = arena_alloc_array(arena, array->count, u8);
            } break;

            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_SHORT: {
                accessor->min = arena_alloc_array(arena, array->count, i16);
            } break;

            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_SHORT: {
                accessor->min = arena_alloc_array(arena, array->count, u16);
            } break;
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_INT: {
                accessor->min = arena_alloc_array(arena, array->count, u32);
            } break;
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT: {
                accessor->min = arena_alloc_array(arena, array->count, f32);
            } break;
            case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNKNOWN: {
                accessor->min = arena_alloc_array(arena, array->count, f32);
            } break;
            }

            PoneJsonArrayValue *array_value = array->values;
            for (usize min_index = 0; min_index < array->count; ++min_index) {
                PONE_ASSERT(array_value);

                switch (accessor->component_type) {
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_BYTE: {
                    ((i8 *)accessor->min)[min_index] =
                        (i8)array_value->value->number;
                } break;
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_BYTE: {
                    ((u8 *)accessor->min)[min_index] =
                        (u8)array_value->value->number;
                } break;

                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_SHORT: {
                    ((i16 *)accessor->min)[min_index] =
                        (i16)array_value->value->number;
                } break;

                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_SHORT: {
                    ((u16 *)accessor->min)[min_index] =
                        (u16)array_value->value->number;
                } break;
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_INT: {
                    ((u32 *)accessor->min)[min_index] =
                        (u32)array_value->value->number;
                } break;
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT: {
                    ((f32 *)accessor->min)[min_index] =
                        (f32)array_value->value->number;
                } break;
                case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNKNOWN: {
                    ((f32 *)accessor->min)[min_index] =
                        (f32)array_value->value->number;
                } break;
                }

                array_value = array_value->next;
            }
        } else if (pone_string_eq(
                       name, (PoneString){.buf = (u8 *)"name", .len = 4})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_STRING);
            accessor->name =
                (PoneString *)arena_alloc(arena, sizeof(PoneString));
            accessor->name->buf = (u8 *)arena_alloc(arena, value->string.len);
            accessor->name->len = value->string.len;

            pone_memcpy((void *)accessor->name->buf, (void *)value->string.buf,
                        accessor->name->len);
        }
    }

    PONE_ASSERT(accessor->component_type !=
                PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNKNOWN);
    PONE_ASSERT(accessor->count);
}

static void pone_gltf_parse_accessors(PoneJsonArray *array, Arena *arena,
                                      PoneGltfAccessor *accessors) {
    PoneJsonArrayValue *array_value = array->values;
    for (usize accessor_index = 0; accessor_index < array->count;
         ++accessor_index) {
        PONE_ASSERT(array_value);
        PONE_ASSERT(array_value->value->type == PONE_JSON_TYPE_OBJECT);

        pone_gltf_parse_accessor(&array_value->value->object, arena,
                                 &accessors[accessor_index]);

        array_value = array_value->next;
    }
}

static void pone_gltf_parse_mesh_primitive_attributes(
    PoneJsonObject *object, Arena *arena,
    PoneGltfMeshPrimitiveAttribute *attributes) {
    PoneJsonPair *pair = object->pairs;
    for (usize attribute_index = 0; attribute_index < object->count;
         ++attribute_index) {
        PONE_ASSERT(pair);
        PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_NUMBER);
        PoneGltfMeshPrimitiveAttribute *attribute =
            &attributes[attribute_index];

        u8 *buf = arena_alloc_array(arena, pair->name.len, u8);
        attribute->name.buf = buf;
        attribute->name.len = pair->name.len;
        pone_memcpy((void *)attribute->name.buf, (void *)pair->name.buf,
                    attribute->name.len);

        attribute->index = (usize)pair->value->number;
        pair = pair->next;
    }
}

static void pone_gltf_parse_mesh_primitive(PoneJsonObject *object, Arena *arena,
                                           PoneGltfMeshPrimitive *primitive) {
    primitive->attributes = 0;
    primitive->attribute_count = 0;
    primitive->mode = PONE_GLTF_TOPOLOGY_TYPE_TRIANGLES;

    for (PoneJsonPair *pair = object->pairs; pair; pair = pair->next) {
        PoneString name = pair->name;
        PoneJsonValue *value = pair->value;

        if (pone_string_eq(
                name, (PoneString){.buf = (u8 *)"attributes", .len = 10})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_OBJECT);

            primitive->attributes = arena_alloc_array(
                arena, value->object.count, PoneGltfMeshPrimitiveAttribute);
            primitive->attribute_count = value->object.count;
            pone_gltf_parse_mesh_primitive_attributes(&value->object, arena,
                                                      primitive->attributes);
        } else if (pone_string_eq(
                       name, (PoneString){.buf = (u8 *)"indices", .len = 7})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_NUMBER);

            primitive->indices = (u32 *)arena_alloc(arena, sizeof(u32));

            *primitive->indices = (u32)value->number;
        } else if (pone_string_eq(
                       name, (PoneString){.buf = (u8 *)"material", .len = 8})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_NUMBER);

            primitive->material = (u32 *)arena_alloc(arena, sizeof(u32));

            *primitive->material = (u32)value->number;
        } else if (pone_string_eq(
                       name, (PoneString){.buf = (u8 *)"mode", .len = 4})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_NUMBER);
            primitive->mode = (PoneGltfTopologyType)(u32)value->number;
        }
    }

    PONE_ASSERT(primitive->attributes);
    PONE_ASSERT(primitive->attribute_count);
}

static void pone_gltf_parse_mesh_primitives(PoneJsonArray *array, Arena *arena,
                                            PoneGltfMeshPrimitive *primitives) {
    PoneJsonArrayValue *array_value = array->values;
    for (usize primitive_index = 0; primitive_index < array->count;
         ++primitive_index) {
        PONE_ASSERT(array_value);
        PONE_ASSERT(array_value->value->type == PONE_JSON_TYPE_OBJECT);

        pone_gltf_parse_mesh_primitive(&array_value->value->object, arena,
                                       &primitives[primitive_index]);

        array_value = array_value->next;
    }
}

static void pone_gltf_parse_mesh(PoneJsonObject *object, Arena *arena,
                                 PoneGltfMesh *mesh) {
    mesh->primitives = 0;
    mesh->primitive_count = 0;
    mesh->weights = 0;
    mesh->weight_count = 0;
    mesh->name = 0;

    for (PoneJsonPair *pair = object->pairs; pair; pair = pair->next) {
        PoneString name = pair->name;
        PoneJsonValue *value = pair->value;

        if (pone_string_eq(name, {.buf = (u8 *)"primitives", .len = 10})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_ARRAY);
            mesh->primitives = arena_alloc_array(arena, value->array.count,
                                                 PoneGltfMeshPrimitive);
            mesh->primitive_count = value->array.count;

            pone_gltf_parse_mesh_primitives(&value->array, arena,
                                            mesh->primitives);
        } else if (pone_string_eq(name, {.buf = (u8 *)"weights", .len = 7})) {
            PONE_ASSERT(0);
        } else if (pone_string_eq(name, {.buf = (u8 *)"name", .len = 4})) {
            PONE_ASSERT(value->type == PONE_JSON_TYPE_STRING);
            mesh->name = (PoneString *)arena_alloc(arena, sizeof(PoneString));
            mesh->name->buf = (u8 *)arena_alloc(arena, value->string.len);
            mesh->name->len = value->string.len;
            pone_memcpy((void *)mesh->name->buf, (void *)value->string.buf,
                        mesh->name->len);
        }
    }
    PONE_ASSERT(mesh->primitives);
    PONE_ASSERT(mesh->primitive_count);
}

static void pone_gltf_parse_meshes(PoneJsonArray *array, Arena *arena,
                                   PoneGltfMesh *meshes) {
    PoneJsonArrayValue *array_value = array->values;
    for (usize mesh_index = 0; mesh_index < array->count; ++mesh_index) {
        PONE_ASSERT(array_value);
        PONE_ASSERT(array_value->value->type == PONE_JSON_TYPE_OBJECT);

        pone_gltf_parse_mesh(&array_value->value->object, arena,
                             &meshes[mesh_index]);

        array_value = array_value->next;
    }
}

static void pone_gltf_parse_buffer(PoneJsonObject *object, Arena *arena,
                                   PoneGltfBuffer *buffer) {
    PoneJsonPair *pair = object->pairs;
    for (usize buffer_index = 0; buffer_index < object->count; ++buffer_index) {
        PONE_ASSERT(pair);

        if (pone_string_eq(pair->name, {.buf = (u8 *)"uri", .len = 3})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_STRING);
            PoneString *uri =
                (PoneString *)arena_alloc(arena, sizeof(PoneString));
            uri->buf = arena_alloc_array(arena, pair->value->string.len, u8);
            uri->len = pair->value->string.len;

            buffer->uri = uri;
        } else if (pone_string_eq(pair->name,
                                  {.buf = (u8 *)"byteLength", .len = 10})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_NUMBER);

            buffer->byte_length = (usize)pair->value->number;
        } else if (pone_string_eq(pair->name,
                                  {.buf = (u8 *)"byteLength", .len = 10})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_STRING);
            PoneString *name =
                (PoneString *)arena_alloc(arena, sizeof(PoneString));
            name->buf = arena_alloc_array(arena, pair->value->string.len, u8);
            name->len = pair->value->string.len;

            buffer->name = name;
        }

        pair = pair->next;
    }
}

static void pone_gltf_parse_buffers(PoneJsonArray *array, Arena *arena,
                                    PoneGltfBuffer *buffers) {
    PoneJsonArrayValue *array_value = array->values;
    for (usize buffer_index = 0; buffer_index < array->count; ++buffer_index) {
        PONE_ASSERT(array_value);
        PONE_ASSERT(array_value->value->type == PONE_JSON_TYPE_OBJECT);
        pone_gltf_parse_buffer(&array_value->value->object, arena,
                               &buffers[buffer_index]);

        array_value = array_value->next;
    }
}

static void pone_gltf_parse_buffer_view(PoneJsonObject *object, Arena *arena,
                                        PoneGltfBufferView *buffer_view) {
    b8 buffer_found = 0;
    b8 byte_length_found = 0;
    buffer_view->byte_offset = 0;
    buffer_view->byte_stride = 0;
    buffer_view->target = PONE_GLTF_BUFFER_TYPE_NONE;

    PoneJsonPair *pair = object->pairs;
    for (usize pair_index = 0; pair_index < object->count; ++pair_index) {
        PONE_ASSERT(pair);

        if (pone_string_eq(pair->name, {.buf = (u8 *)"buffer", .len = 6})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_NUMBER);
            buffer_view->buffer = (usize)pair->value->number;
            buffer_found = 1;
        } else if (pone_string_eq(pair->name,
                                  {.buf = (u8 *)"byteOffset", .len = 10})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_NUMBER);
            buffer_view->byte_offset = (usize)pair->value->number;
        } else if (pone_string_eq(pair->name,
                                  {.buf = (u8 *)"byteLength", .len = 10})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_NUMBER);
            buffer_view->byte_length = (usize)pair->value->number;
            byte_length_found = 1;
        } else if (pone_string_eq(pair->name,
                                  {.buf = (u8 *)"byteStride", .len = 10})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_NUMBER);
            buffer_view->byte_stride = (usize)pair->value->number;
            PONE_ASSERT(buffer_view->byte_stride >= 4 &&
                        buffer_view->byte_stride <= 252);
        } else if (pone_string_eq(pair->name,
                                  {.buf = (u8 *)"target", .len = 6})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_NUMBER);
            buffer_view->target = (PoneGltfBufferType)(u32)pair->value->number;
        } else if (pone_string_eq(pair->name,
                                  {.buf = (u8 *)"name", .len = 4})) {
            PONE_ASSERT(pair->value->type == PONE_JSON_TYPE_STRING);
            PoneString *name =
                (PoneString *)arena_alloc(arena, sizeof(PoneString));
            name->buf = arena_alloc_array(arena, pair->value->string.len, u8);
            name->len = pair->value->string.len;

            buffer_view->name = name;
        }

        pair = pair->next;
    }

    PONE_ASSERT(buffer_found && byte_length_found);
}

static void pone_gltf_parse_buffer_views(PoneJsonArray *array, Arena *arena,
                                         PoneGltfBufferView *buffer_views) {
    PoneJsonArrayValue *array_value = array->values;
    for (usize buffer_view_index = 0; buffer_view_index < array->count;
         ++buffer_view_index) {
        PONE_ASSERT(array_value);
        PONE_ASSERT(array_value->value->type == PONE_JSON_TYPE_OBJECT);

        pone_gltf_parse_buffer_view(&array_value->value->object, arena,
                                    &buffer_views[buffer_view_index]);

        array_value = array_value->next;
    }
}

static void pone_gltf_get_acccessor_data(PoneGltf *gltf, usize accessor_index,
                                         void **data, usize *data_size) {
    PONE_ASSERT(accessor_index < gltf->accessor_count);
    PoneGltfAccessor *accessor = &gltf->accessors[accessor_index];
    PONE_ASSERT(accessor->buffer_view);
    PoneGltfBufferView *buffer_view =
        &gltf->buffer_views[*accessor->buffer_view];

    usize start = accessor->byte_offset + buffer_view->byte_offset;
    usize stride;
    if (buffer_view->byte_stride) {
        stride = buffer_view->byte_stride;
    } else {
        stride = 1;
        switch (accessor->component_type) {
        case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_SHORT:
        case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_SHORT: {
            stride *= 2;
        } break;
        case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_INT:
        case PONE_GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT: {
            stride *= 4;
        } break;
        default: {
        } break;
        }

        switch (accessor->type) {
        case PONE_GLTF_ACCESSOR_TYPE_VEC2: {
            stride *= 2;
        } break;
        case PONE_GLTF_ACCESSOR_TYPE_VEC3: {
            stride *= 3;
        } break;
        case PONE_GLTF_ACCESSOR_TYPE_VEC4:
        case PONE_GLTF_ACCESSOR_TYPE_MAT2: {
            stride *= 4;
        } break;
        case PONE_GLTF_ACCESSOR_TYPE_MAT3: {
            stride *= 9;
        } break;
        case PONE_GLTF_ACCESSOR_TYPE_MAT4: {
            stride *= 16;
        } break;
        default: {
        } break;
        }
    }

    *data_size = stride * accessor->count;
    *data = (void *)((u8 *)gltf->data + start);
}

void pone_gltf_get_mesh_primitive_attribute_data(
    PoneGltf *gltf, usize mesh_index, usize mesh_primitive_index,
    PoneString attribute_name, void **data, usize *data_size) {
    PONE_ASSERT(mesh_index < gltf->mesh_count);
    PoneGltfMesh *mesh = &gltf->meshes[mesh_index];
    PONE_ASSERT(mesh_primitive_index < mesh->primitive_count);
    PoneGltfMeshPrimitive *mesh_primitive =
        &mesh->primitives[mesh_primitive_index];

    PoneGltfMeshPrimitiveAttribute *attribute = 0;
    for (usize attribute_index = 0;
         attribute_index < mesh_primitive->attribute_count; ++attribute_index) {
        if (pone_string_eq(attribute_name,
                           mesh_primitive->attributes[attribute_index].name)) {
            attribute = &mesh_primitive->attributes[attribute_index];
        }
    }
    PONE_ASSERT(attribute);

    pone_gltf_get_acccessor_data(gltf, attribute->index, data, data_size);
}

void pone_gltf_get_mesh_primitive_index_data(PoneGltf *gltf, usize mesh_index,
                                             usize mesh_primitive_index,
                                             void **data, usize *data_size) {
    PONE_ASSERT(mesh_index < gltf->mesh_count &&
                mesh_primitive_index <
                    gltf->meshes[mesh_index].primitive_count);

    PoneGltfMesh *mesh = gltf->meshes + mesh_index;
    PoneGltfMeshPrimitive *mesh_primitive =
        mesh->primitives + mesh_primitive_index;

    PONE_ASSERT(mesh_primitive->indices);
    pone_gltf_get_acccessor_data(gltf, *mesh_primitive->indices, data,
                                 data_size);
}

PoneGltf *pone_gltf_parse(void *data, Arena *arena) {
    PoneGltf *gltf = (PoneGltf *)arena_alloc(arena, sizeof(PoneGltf));

    pone_gltf_parse_header(data, &gltf->header);
    usize offset = 12;

    while (offset < gltf->header.length) {
        void *p = (void *)((u8 *)data + offset);
        PoneGltfChunk chunk;
        pone_gltf_parse_chunk(p, &chunk);

        if (chunk.type == PONE_GLTF_CHUNK_TYPE_JSON) {
            PoneJsonScanner scanner = {
                .input = {.buf = chunk.data, .len = chunk.length},
                .cursor = 0,
            };
            PoneJsonValue *gltf_value =
                pone_json_scanner_parse_value(&scanner, arena);
            PONE_ASSERT(gltf_value->type == PONE_JSON_TYPE_OBJECT);
            for (PoneJsonPair *pair = gltf_value->object.pairs; pair;
                 pair = pair->next) {
                PoneString name = pair->name;
                PoneJsonValue *value = pair->value;

                if (pone_string_eq(name, (PoneString){.buf = (u8 *)"accessors",
                                                      .len = 9})) {
                    PONE_ASSERT(value->type == PONE_JSON_TYPE_ARRAY);

                    gltf->accessors = arena_alloc_array(
                        arena, value->array.count, PoneGltfAccessor);
                    gltf->accessor_count = value->array.count;

                    pone_gltf_parse_accessors(&value->array, arena,
                                              gltf->accessors);
                } else if (pone_string_eq(name,
                                          {.buf = (u8 *)"meshes", .len = 6})) {
                    PONE_ASSERT(value->type == PONE_JSON_TYPE_ARRAY);

                    gltf->meshes = arena_alloc_array(arena, value->array.count,
                                                     PoneGltfMesh);
                    gltf->mesh_count = value->array.count;

                    pone_gltf_parse_meshes(&value->array, arena, gltf->meshes);
                } else if (pone_string_eq(
                               name, {.buf = (u8 *)"bufferViews", .len = 11})) {
                    PONE_ASSERT(value->type == PONE_JSON_TYPE_ARRAY);

                    usize buffer_view_count = value->array.count;
                    PoneGltfBufferView *buffer_views = arena_alloc_array(
                        arena, buffer_view_count, PoneGltfBufferView);

                    pone_gltf_parse_buffer_views(&value->array, arena,
                                                 buffer_views);

                    gltf->buffer_views = buffer_views;
                    gltf->buffer_count = buffer_view_count;
                } else if (pone_string_eq(name,
                                          {.buf = (u8 *)"buffers", .len = 7})) {
                    PONE_ASSERT(value->type == PONE_JSON_TYPE_ARRAY);

                    usize buffer_count = value->array.count;
                    PoneGltfBuffer *buffers =
                        arena_alloc_array(arena, buffer_count, PoneGltfBuffer);

                    pone_gltf_parse_buffers(&value->array, arena, buffers);

                    gltf->buffers = buffers;
                    gltf->buffer_count = buffer_count;
                }
            }
        } else if (chunk.type == PONE_GLTF_CHUNK_TYPE_BIN) {
            usize data_size = chunk.length;
            u8 *data = arena_alloc_array(arena, data_size, u8);
            pone_memcpy((void *)data, (void *)chunk.data, data_size);

            gltf->data = data;
            gltf->data_size = data_size;
        }

        offset += 2 * sizeof(u32) + chunk.length;
    }

    return gltf;
}
