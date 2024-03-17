#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

/** NOTE: Importing & Serialization code
 * In infancy
*/

typedef struct blinn_mr_instance {
    v4s color_factors;
    u32 color_tex_index;
    f32 metalness;
    f32 roughness;
    u32 mr_tex_index;
} blinn_mr_instance;

typedef enum sampler_property_flag_bits {
    SAMPLER_PROPERTY_ANISOTROPIC = 0x01,
    SAMPLER_PROPERTY_NEAREST = 0x02,
    SAMPLER_PROPERTY_LINEAR = 0x04,
    SAMPLER_PROPERTY_MIPMAP_NEAREST = 0x08,
    SAMPLER_PROPERTY_MIPMAP_LINEAR = 0x10,
} sampler_property_flag_bits;
typedef u32 sampler_property_flags;

typedef struct import_geometry {
    vertex* vertices;       // dynarray, TODO: Regular allocation
    u32* indices;           // dynarray, TODO: Regular allocation
} import_geometry;

typedef struct import_image {
    u32 height;
    u32 width;
    u32 channels;
    void* data;             // stb_image allocation
    char* name;
} import_image;

typedef struct import_sampler {
    u8 mag_filter;
    u8 min_filter;
    u8 mip_map_mode;
} import_sampler;

// TODO: Add sampler information. Use default linear for now
typedef struct import_texture {
    u32 image_id;
    u32 sampler_id;
} import_texture;

typedef enum import_pipeline_type {
    IMPORT_PIPELINE_GLTF_DEFAULT = 0,
    IMPORT_PIPELINE_GLTF_TRANSPARENT,
    IMPORT_PIPELINE_DEFAULT_MAX,
} import_pipeline_type;

// NOTE: Default shaders for importing,
// as file formats do not include shaders,
typedef struct import_pipeline {
    const char* vert_path;
    const char* frag_path;
    void* instances;
    u64 inst_size;
    import_pipeline_type type;
    b8 transparent;
} import_pipeline;

const static import_pipeline default_import_pipelines[IMPORT_PIPELINE_DEFAULT_MAX] = {
    [IMPORT_PIPELINE_GLTF_DEFAULT] = {
        .vert_path = "assets/shaders/blinn_mr.vert.spv.opt",
        .frag_path = "assets/shaders/blinn_mr.frag.spv.opt",
        .inst_size = sizeof(blinn_mr_instance),
        .type = IMPORT_PIPELINE_GLTF_DEFAULT,
        .instances = NULL,
        .transparent = false,
    },
    [IMPORT_PIPELINE_GLTF_TRANSPARENT] = {
        .vert_path = "assets/shaders/blinn_mr.vert.spv.opt",
        .frag_path = "assets/shaders/blinn_mr.frag.spv.opt",
        .inst_size = sizeof(blinn_mr_instance),
        .type = IMPORT_PIPELINE_GLTF_TRANSPARENT,
        .instances = NULL,
        .transparent = true,
    },
};

// TODO: Have materials be per instance of a mesh, as we are bindless and can do that
// Currently still figuring out aspects like animation so it can wait.
typedef struct import_mesh {
    u32* geometry_indices;      // dynarray
    u32* material_indices;      // dynarray
    u32 count;
} import_mesh;

typedef struct import_node {
    b8 has_mesh;
    b8 has_parent;
    u32 mesh_index;
    u32 parent_index;
    u32* children_indices;      // dynarray
    m4s local_transform;
    m4s world_transform;
} import_node;

// NOTE: All dynarrays
typedef struct import_payload {
    mat_id* mat_index_to_id;           // Dynarray
    import_pipeline* pipelines;         // Dynarray
    
    import_geometry* geometries;        // Dynarray
    import_image* images;               // Dynarray
    import_sampler* samplers;
    import_texture* textures;           // Dynarray
    import_mesh* meshes;                // Dynarray

    import_node* nodes;                 // Dynarray
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
    u32 image_index;
    // Use default sampler for now
} serialized_texture;

typedef struct serialized_mat_pipe {
    blob_view vert;         // Vertex shader blob view, this is a path to the vertex shader, null terminated
    blob_view frag;         // Fragment shader blob view, this is a path to the fragment shader, null terminated
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
