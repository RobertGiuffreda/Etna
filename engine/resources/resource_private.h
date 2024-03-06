#pragma once
#include "resource_types.h"

#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"

#define MAX_MESH_COUNT 512
#define MAX_MATERIAL_BLUEPRINT_COUNT 8
#define MAX_MATERIAL_COUNT 256
#define MAX_IMAGE_COUNT 128

// TODO: typedef id types for each resource

// Big index buffer and vertex buffer
// Add bindless functionality 
// Remove regular

struct image_manager {
    renderer_state* state;
    
    image images[MAX_IMAGE_COUNT];
    u32 image_count;
};

// typedef struct mat_id {
//     u32 pipe_id;    // Pipeline Created from materials shaders
//     u32 inst_id;
// } mat_id;

// typedef struct mat_pipe {
//     VkPipeline pipeline;
//     VkPipelineLayout layout;
//     VkDescriptorSet set;
// } mat_pipe;

// typedef struct mat {
//     mat_pipe pipe;
//     VkDescriptorSetLayout set_layout;
//     u64 inst_block_size;
//     u32 inst_count;
//     buffer inst_data_buffer;
//     void* inst_data;         // TEMP: GPU only memory in the future, wont be mapped
// } mat;

// // TODO: Handle storage buffer having data variables that precurse a variable size array
// typedef struct mat_binding {
//     VkDescriptorType type;
//     VkShaderStageFlags stages;
//     u32 binding;
//     u32 count;          // 0 if variable length array
//     u32 size;           // Size of uniform block or ssbo
// } mat_binding;
// // TODO: END

// typedef struct mat_shader {
//     VkShaderModule vert;
//     VkShaderModule frag;

//     u32 binding_count;
//     mat_binding* bindings;
// } mat_shader;

// typedef struct mat_config {
//     const char* vert_path;
//     const char* frag_path;
//     scene* scene;
//     renderer_state* state;
//     // TEMP: Until material shader reflection data is used
//     u64 inst_data_size;
//     // TEMP: END
//     b8 transparent;
// } mat_config;

// b8 load_mat_shader(
//     const char* vert_path,
//     const char* frag_path,
//     renderer_state* state,
//     mat_shader* shader);

// b8 mat_init(mat_config* config, mat* material);
// void mat_shutdown(mat* material, renderer_state* state);

// // NOTE: Returns material instance id
// u32 mat_instance_create(renderer_state* state, mat* material, u64 data_size, void* data);