#pragma once
#include "defines.h"
#include "math/math_types.h"

/** NOTE: Importing & Serialization code
 * In infancy
*/

typedef enum import_file_type {
    IMPORT_FILE_GLTF,
    IMPORT_FILE_GLB,
} import_file_type;

typedef enum shader_model {
    SHADER_MODEL_PBR = 0,
    SHADER_MODEL_NPR = 1,
} shader_model;

typedef enum pbr_property_flag_bits {
    PBR_PROPERTY_METAL_ROUGH = 0x01,
    PBR_PROPERTY_SPECULAR = 0x02,
} pbr_property_flag_bits;
typedef u32 pbr_property_flags;

typedef enum npr_property_flag_bits {
    NPR_PROPERTY_OUTLINE = 0x01,
    NPR_PROPERTY_TOON = 0x02,
} npr_property_flag_bits;
typedef u32 npr_property_flags;

typedef enum sampler_property_flag_bits {
    SAMPLER_PROPERTY_ANISOTROPIC = 0x01,
    SAMPLER_PROPERTY_NEAREST = 0x02,
    SAMPLER_PROPERTY_LINEAR = 0x04,
    SAMPLER_PROPERTY_MIPMAP_NEAREST = 0x08,
    SAMPLER_PROPERTY_MIPMAP_LINEAR = 0x10,
} sampler_property_flag_bits;
typedef u32 sampler_property_flags;

typedef enum alpha_mode {
    ALPHA_MODE_OPAQUE = 0,
    ALPHA_MODE_MASK,
    ALPHA_MODE_BLEND,
    ALPHA_MODE_UNKNOWN,
} alpha_mode;

typedef struct imported_geometry {
    vertex* vertices;       // dynarray
    u32* indices;           // dynarray
} imported_geometry;

typedef struct imported_image {
    u32 height;
    u32 width;
    u32 channels;
    void* data;
} imported_image;

// TODO: Add sampler information.
// Just use default linear for now
typedef struct imported_texture {
    u32 image_id;
} imported_texture;

// Modeled after gltf as that is what I am loading for the forseable future
typedef struct imported_material {
    v4s color_factors;
    i32 color_tex_id;  // -1 if not present
    f32 metalness;
    f32 roughness;
    i32 mr_tex_id;     // -1 if not present
    shader_model shading_model;
    union {
        pbr_property_flags pbr;
        npr_property_flags npr;
    } flags;
    alpha_mode alpha;
} imported_material;

// TODO: Have materials be per instance of a mesh, as we are bindless and can do that
// Currently still figuring out aspects like animation so it can wait.
typedef struct imported_mesh {
    u32* geometry_ids;      // dynarray
    u32* material_ids;      // dynarray
    u32 count;
} imported_mesh;

typedef struct imported_node {
    b8 has_mesh;
    b8 has_parent;
    u32 mesh_id;
    u32 parent_id;
    u32* children_ids;      // dynarray
    m4s local_transform;
    m4s world_transform;
} imported_node;

// NOTE: All dynarrays
typedef struct import_payload {
    imported_geometry* geometries;
    imported_image* images;
    imported_texture* textures;
    imported_material* materials;
    imported_mesh* meshes;

    imported_node* nodes;
} import_payload;

// Serializing Beginning
typedef enum index_type {
    INDEX_TYPE_U32 = 0,
    INDEX_TYPE_U16,
} index_type;

typedef enum vertex_type {
    VERTEX_TYPE_STATIC = 0,
    VERTEX_TYPE_ANIMATED,
} vertex_type;

typedef struct blob_view {
    u64 offset;
    u64 range;
} blob_view;

#define HEADER_SIZE 512
typedef union binary_header {
    const u8 _bytes[HEADER_SIZE];
    struct {
        blob_view verts;        // Ready to copy into GPU vertex buffer
        blob_view indices;      // Ready to copy into GPU index buffer
        blob_view geos;         // Ready to copy into GPU geometry buffer
        blob_view transforms;   // Ready to copy into GPU transform buffer
        blob_view objects;      // Ready to copy into GPU object buffer
        blob_view images;       // Array of serialized_image
        blob_view textures;     // Array of serialized_texture
        blob_view mat_pipes;    // Array of serialized_mat_pipe
        blob_view meshes;       // Array of serialized_mesh
    };
} binary_header;

typedef struct serialized_image {
    u32 width;
    u32 height;
    blob_view image;    // Image data ready to load into buffer to transfer to image
} serialized_image;

typedef struct serialized_texture {
    u32 image;
    // Use default sampler for now
} serialized_texture;

typedef struct serialized_mat_pipe {
    blob_view vert;         // Vertex shader blob view for this material pipeline
    blob_view frag;         // Fragment shader blob view for this material pipeline
    blob_view instances;    // Instance buffer directly ready to load into buffer with memcpy
} serialized_mat_pipe;

typedef struct serialized_geometry {
    u32 start_index;
    u32 index_count;
    i32 vertex_offset;
} serialized_geometry;

typedef struct serialized_mesh {
    u32 geo_count;
    blob_view geo_indices;  // Array of serialized geometry objects
} serialized_mesh;
