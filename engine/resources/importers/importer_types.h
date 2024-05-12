#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "resources/resource_types.h"

// TEMP: Transforms
#include "scene/transforms.h"
// TEMP: END

// Currently gltf_default & gltf_transparent
typedef struct pbr_mr_instance {
    v4s color_factors;
    u32 color_tex_index;
    f32 metalness;
    f32 roughness;
    u32 mr_tex_index;
    u32 normal_index;
} pbr_mr_instance;

typedef struct cel_instance {
    v4s color_factors;
    u32 color_tex_index;
} cel_instance;

typedef enum sampler_property_flag_bits {
    SAMPLER_PROPERTY_ANISOTROPIC = 0x01,
    SAMPLER_PROPERTY_NEAREST = 0x02,
    SAMPLER_PROPERTY_LINEAR = 0x04,
    SAMPLER_PROPERTY_MIPMAP_NEAREST = 0x08,
    SAMPLER_PROPERTY_MIPMAP_LINEAR = 0x10,
} sampler_property_flag_bits;
typedef u32 sampler_property_flags;

typedef struct import_image {
    char* name;
    void* data;             // stb_image allocation
    u32 width;
    u32 height;
    u32 channels;
} import_image;

// HACK: Quick version, transfers vulkan enum values from importing to sampler creation
typedef struct import_sampler {
    u8 mag_filter;
    u8 min_filter;
    u8 mip_map_mode;
} import_sampler;
// HACK: END

typedef enum reserved_texture_index {
    RESERVED_TEXTURE_ERROR_INDEX = 0,
    RESERVED_TEXTURE_WHITE_INDEX,
    RESERVED_TEXTURE_BLACK_INDEX,
    RESERVED_TEXTURE_NORMAL_INDEX,
    RESERVED_TEXTURE_SHADOW_MAP_INDEX,
    RESERVED_TEXTURE_INDEX_COUNT,
} reserved_texture_index;

typedef struct import_texture {
    u32 image_id;
    u32 sampler_id;
} import_texture;

typedef enum import_pipeline_type {
    IMPORT_PIPELINE_TYPE_GLTF_DEFAULT = 0,
    IMPORT_PIPELINE_TYPE_PMX_DEFAULT,
    IMPORT_PIPELINE_TYPE_GLTF_TRANSPARENT,
    IMPORT_PIPELINE_TYPE_PMX_TRANSPARENT,
    IMPORT_PIPELINE_TYPE_COUNT,
} import_pipeline_type;

// TODO: Consolidate: This is now the same 
// struct as mat_pipe_config
typedef struct import_pipeline {
    const char* vert_path;
    const char* frag_path;
    void* instances;
    u64 inst_size;
    b8 transparent;
} import_pipeline;

static const import_pipeline default_import_pipelines[IMPORT_PIPELINE_TYPE_COUNT] = {
    [IMPORT_PIPELINE_TYPE_GLTF_DEFAULT] = {
        .vert_path = "assets/shaders/pbr_mr.vert.spv.opt",
        .frag_path = "assets/shaders/pbr_mr.frag.spv.opt",
        .inst_size = sizeof(pbr_mr_instance),
        .instances = NULL,
        .transparent = false,
    },
    // TODO: Get shadow information in shaders
    [IMPORT_PIPELINE_TYPE_PMX_DEFAULT] = {
        .vert_path = "assets/shaders/cel.vert.spv.opt",
        .frag_path = "assets/shaders/cel.frag.spv.opt",
        .inst_size = sizeof(cel_instance),
        .instances = NULL,
        .transparent = false,
    },
    [IMPORT_PIPELINE_TYPE_GLTF_TRANSPARENT] = {
        .vert_path = "assets/shaders/pbr_mr.vert.spv.opt",
        .frag_path = "assets/shaders/pbr_mr.frag.spv.opt",
        .inst_size = sizeof(pbr_mr_instance),
        .instances = NULL,
        .transparent = true,
    },
    // TODO: Get shadow information in shaders
    [IMPORT_PIPELINE_TYPE_PMX_TRANSPARENT] = {
        .vert_path = "assets/shaders/cel.vert.spv.opt",
        .frag_path = "assets/shaders/cel.frag.spv.opt",
        .inst_size = sizeof(cel_instance),
        .instances = NULL,
        .transparent = true,
    },
};

static const pbr_mr_instance default_pbr_instance = {
    .color_factors = {.raw = {1.f, 1.f, 1.f, 1.f}},
    .color_tex_index = RESERVED_TEXTURE_ERROR_INDEX,
    .metalness = 0.0f,
    .roughness = 0.5f,
    .mr_tex_index = RESERVED_TEXTURE_WHITE_INDEX,
    .normal_index = RESERVED_TEXTURE_NORMAL_INDEX,
};

static const cel_instance default_cel_instance = {
    .color_factors = {.raw = {1.f, 1.f, 1.f, 1.f}},
    .color_tex_index = RESERVED_TEXTURE_ERROR_INDEX,
};

static const void* default_pipeline_default_instances[IMPORT_PIPELINE_TYPE_COUNT] = {
    [IMPORT_PIPELINE_TYPE_GLTF_DEFAULT] = &default_pbr_instance,
    [IMPORT_PIPELINE_TYPE_PMX_DEFAULT] = &default_cel_instance,
    [IMPORT_PIPELINE_TYPE_GLTF_TRANSPARENT] = &default_pbr_instance,
    [IMPORT_PIPELINE_TYPE_PMX_TRANSPARENT] = &default_cel_instance,
};

typedef struct import_skin {
    u32* joint_indices;         // dynarray
    m4s* inverse_binds;         // dynarray
} import_skin;

typedef struct import_node {
    b8 has_parent;
    b8 has_mesh;
    b8 has_skin;
    u32 parent_index;
    u32 mesh_index;
    u32 skin_index;
    u32* children_indices;      // dynarray
    m4s local_transform;
    m4s world_transform;
} import_node;

// TEMP: The engine is structured to accommodate 
// GLTF's style of skinning and animations until
// I have a handle on how these things work
typedef enum transform_type {
    TRANSFORM_TYPE_TRANSLATION = 1, // Starts at one to match with cgltf atm
    TRANSFORM_TYPE_ROTATION,
    TRANSFORM_TYPE_SCALE,
    TRANSFORM_TYPE_WEIGHTS,
} transform_type;

typedef enum interpolation_type {
    INTERPOLATION_TYPE_LINEAR = 0,
    INTERPOLATION_TYPE_STEP,
    INTERPOLATION_TYPE_CUBIC_SPLINE,
} interpolation_type;

typedef struct import_anim_channel {
    u32 sampler_index;
    u32 target_index;
    transform_type trans_type;
} import_anim_channel;

typedef struct import_anim_sampler {
    f32* timestamps;
    v4s* data;
    interpolation_type interp_type;
} import_anim_sampler;

typedef struct import_animation {
    f32 duration;
    import_anim_channel* channels;
    import_anim_sampler* samplers;
} import_animation;
// TEMP: END

// TODO: Meshes w/ Morph targets w/o skinning info
// stat_mesh  - no morph targets and no skinning info
// morph_mesh -    morph targets and no skinning info
// skin_mesh  - no morph targets and    skinning info
// anim_mesh  -    morph targets and    skinning info

// static_mesh + morph_mesh pull from non-skin vertices
// skin_mesh + anim_mesh pull from skinned vertices

typedef enum {
    VERTEX_STATIC = 0,    // Mesh vertices are static
    VERTEX_MORPHING,      // Mesh vertices modified via morph targets
    VERTEX_SKINNED,       // Mesh vertices modified via skinning
    VERTEX_SKIN_MORPH,    // Mesh vertices modified via skinning & morph targets
} vertex_e;

typedef struct {
    u32 index_start;
    u32 index_count;
    f32 radius;             // Bounding sphere radius
    u32 padd;
    v4s origin;             // Bounding box origin
    v4s extent;             // Bounding box extent
} import_geometry;

// NOTE: Morphs: 2 verts, 3 morph_targets, position & normal
//|P0P1P2|N0N1N2|P0P1P2|N0N1N2|
//|   Vertex 1  |  Vertex 2   |
typedef struct {
    vertex_e vertex_type;
    u32 morph_count;
    void* morph_targets;
    f32* morph_weights;
    u32 vertex_offset;
    u32 vertex_count;
    u32 geo_count;
    u32* geometries;
    mat_id* materials;
} import_mesh;

/** NOTE:
 * The rendering system is still GLTF centric in regards to the node logic. 
 * Node itself has been replaced with transforms but the logic is still the same.
 * 
 * This means that because skins and animation data both target nodes(transforms) that are
 * present in the scene, in order to represent the same mesh with the same skinning rig in a 
 * different animation state, the animations and skins would have to refer to different nodes
 * in the scene, even though it is the same data. At least I think.
 */

// TEMP: See above, until real skin instancing can be accomplished
typedef struct {
    u32 skin;      // Pointer to skin
    u32* meshes;   // Pointer to meshes
} import_skin_instance;
// TEMP: END

typedef struct {
    // TODO: Rename these to reflect nature as configs
    import_pipeline* pipelines;             // Dynarray
    import_image* images;                   // Dynarray
    import_sampler* samplers;               // Dynarray
    import_texture* textures;               // Dynarray
    // TODO: END

    struct {
        vertex* statics;                    // Dynarray
        skin_vertex* skinned;               // Dynarray
    } vertices;
    u32* indices;                           // Dynarray
    import_geometry* geometries;            // Dynarray
    import_mesh* meshes;                    // Dynarray

    import_node* nodes;                     // Dynarray
    u32* root_nodes;                        // Dynarray

    import_skin* skins;                     // Dynarray
    import_animation* animations;           // Dynarray

    import_skin_instance* skin_instances;   // Dynarray

    transforms transforms;
} import_payload;
