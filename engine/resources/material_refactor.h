#pragma once
#include "defines.h"
#include "scene/scene.h"
#include "renderer/src/vk_types.h"

typedef enum mat_set_bindings {
    MAT_DRAWS_BINDING = 0,
    MAT_INSTANCES_BINDING,
    MAT_BINDING_MAX,
} mat_set_bindings;

typedef struct mat_id {
    u32 pipe_id;
    u32 inst_id;
} mat_id;

typedef struct mat_pipe {
    // Info for renderer
    VkPipeline pipe;
    VkDescriptorSet set;
    buffer draws_buffer;

    // Data to hold instances
    u64 inst_size;
    void* inst_data;
    u32 inst_count;
    buffer inst_buffer;
} mat_pipe;

typedef struct mat_pipe_config {
    const char* vert_path;
    const char* frag_path;
    // TEMP: Until material shader reflection data is used
    u64 inst_size;
    // TEMP: END
    b8 transparent;
} mat_pipe_config;

b8 mat_init(mat_pipe* material, scene* scene, renderer_state* state, const mat_pipe_config* config);
void mat_shutdown(mat_pipe* material, scene* scene, renderer_state* state);

// NOTE: Returns material instance id
u32 mat_instance_create(mat_pipe* material, renderer_state* state, u64 data_size, void* data);