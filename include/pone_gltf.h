#ifndef PONE_GLTF_H
#define PONE_GLTF_H

#include "pone_arena.h"
#include "pone_string.h"
#include "pone_types.h"

enum PoneGltfAccessorComponentType {
    PONE_GLTF_ACCESSOR_COMPONENT_TYPE_BYTE = 5120,
    PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_BYTE = 5121,
    PONE_GLTF_ACCESSOR_COMPONENT_TYPE_SHORT = 5122,
    PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_SHORT = 5123,
    PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNSIGNED_INT = 5125,
    PONE_GLTF_ACCESSOR_COMPONENT_TYPE_FLOAT = 5126,
    PONE_GLTF_ACCESSOR_COMPONENT_TYPE_UNKNOWN = 0,
};

enum PoneGltfAccessorType {
    PONE_GLTF_ACCESSOR_TYPE_SCALAR,
    PONE_GLTF_ACCESSOR_TYPE_VEC2,
    PONE_GLTF_ACCESSOR_TYPE_VEC3,
    PONE_GLTF_ACCESSOR_TYPE_VEC4,
    PONE_GLTF_ACCESSOR_TYPE_MAT2,
    PONE_GLTF_ACCESSOR_TYPE_MAT3,
    PONE_GLTF_ACCESSOR_TYPE_MAT4,
};

enum PoneGltfTopologyType {
    PONE_GLTF_TOPOLOGY_TYPE_POINTS,
    PONE_GLTF_TOPOLOGY_TYPE_LINES,
    PONE_GLTF_TOPOLOGY_TYPE_LINE_LOOP,
    PONE_GLTF_TOPOLOGY_TYPE_LINE_STRIP,
    PONE_GLTF_TOPOLOGY_TYPE_TRIANGLES,
    PONE_GLTF_TOPOLOGY_TYPE_TRIANGLE_STRIP,
    PONE_GLTF_TOPOLOGY_TYPE_TRIANGLE_FAN,
};

struct PoneGltfAccessor {
    u32 *buffer_view;
    u32 byte_offset;
    PoneGltfAccessorComponentType component_type;
    b8 normalized;
    usize count;
    PoneGltfAccessorType type;
    void *max;
    void *min;
    PoneString *name;
};

struct PoneGltfMeshPrimitiveAttribute {
    PoneString name;
    usize index;
};

struct PoneGltfMeshPrimitive {
    PoneGltfMeshPrimitiveAttribute *attributes;
    usize attribute_count;
    u32 *indices;
    u32 *material;
    PoneGltfTopologyType mode;
};

struct PoneGltfMesh {
    PoneGltfMeshPrimitive *primitives;
    usize primitive_count;
    f32 *weights;
    usize weight_count;
    PoneString *name;
};

struct PoneGltfBuffer {
    PoneString *uri;
    usize byte_length;
    PoneString *name;
};

enum PoneGltfBufferType {
    PONE_GLTF_BUFFER_TYPE_ARRAY_BUFFER = 34962,
    PONE_GLTF_BUFFER_TYPE_ELEMENT_ARRAY_BUFFER = 34963,

    PONE_GLTF_BUFFER_TYPE_NONE = 0,
};

struct PoneGltfBufferView {
    usize buffer;
    usize byte_offset;
    usize byte_length;
    usize byte_stride;
    PoneGltfBufferType target;
    PoneString *name;
};

struct PoneGltfHeader {
    u32 version;
    u32 length;
};

enum PoneGltfChunkType {
    PONE_GLTF_CHUNK_TYPE_JSON,
    PONE_GLTF_CHUNK_TYPE_BIN,
};

struct PoneGltfChunk {
    u8 *data;
    usize length;
    PoneGltfChunkType type;
};

struct PoneGltf {
    PoneGltfHeader header;
    PoneGltfAccessor *accessors;
    usize accessor_count;
    PoneGltfMesh *meshes;
    usize mesh_count;
    PoneGltfBufferView *buffer_views;
    usize buffer_view_count;
    PoneGltfBuffer *buffers;
    usize buffer_count;

    void *data;
    usize data_size;
};

PoneGltf *pone_gltf_parse(void *data, Arena *arena);
void pone_gltf_get_mesh_primitive_attribute_data(PoneGltf *gltf,
                                                 usize mesh_index,
                                                 usize mesh_primitive_index,
                                                 PoneString attribute_name,
                                                 void **data, usize *data_size);
void pone_gltf_get_mesh_primitive_index_data(PoneGltf *gltf, usize mesh_index,
                                             usize mesh_primitive_index,
                                             void **data, usize *data_size);

#endif
