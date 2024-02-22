#pragma once
#include "defines.h"

#include "core/camera.h"
#include "math/math_types.h"
#include "renderer/src/vk_types.h"
#include "renderer/src/renderables.h"

#include "resources/resource_private.h"

typedef struct scene {
    char* name;

    // TEMP: Until bindless has taken over
    buffer index_buffer;
    buffer vertex_buffer;
    VkDeviceAddress vb_addr;    // Vertex buffer address
    buffer transform_buffer;
    VkDeviceAddress tb_addr;    // Transform buffer address

    surface_2* surfaces;    // dynarray
    mesh_2* meshes;         // dynarray
    m4s* transforms;        // dynarray
    object* objects;        // dynarray

    // Bindless & indirect descriptor set layout is different so this is here until bindless takeover
    VkDescriptorSetLayout scene_layout;

    // Material blueprint(one uber shader) information for bindless instances:
    VkPipeline opaque_pipeline;
    VkPipeline transparent_pipeline;
    VkPipelineLayout pipeline_layout;

    VkDescriptorSetLayout material_layout;
    VkDescriptorSet material_set;

    VkDescriptorImageInfo* image_infos;
    VkDescriptorBufferInfo* buffer_infos;
    VkWriteDescriptorSet* writes;

    material_2 materials[MAX_MATERIAL_COUNT];
    u32 material_count;
    
    // TEMP: Hardcoded for now, rework when materials get abstraction back
    ds_allocator ds_allocator;

    // Draw commands for indirect bindless.
    draw_command* opaque_draws;
    draw_command* transparent_draws;
    // TEMP: END

    image_manager* image_bank;
    material_manager* material_bank;
    mesh_manager* mesh_bank;

    node** top_nodes;
    u32 top_node_count;

    camera cam;
    scene_data data;

    // TEMP: Leftovers from loaded_gltf not refactored out yet
    mesh_node* mesh_nodes;
    u32 mesh_node_count;
    node* nodes;
    u32 node_count;

    // TODO: Put all sampler combos into renderer_state as
    // defaults to avoid having them here. 
    u32 sampler_count;
    VkSampler* samplers;
    // TODO: END

    // TODO: Put into material manager and suballocate??
    buffer material_buffer;
    // TODO: END

    // TEMP: END

    // For items that need to modified per frame
    // ds_allocator* ds_allocators;
    // buffer* scene_data_buffers;

    renderer_state* state;
} scene;

// TEMP: Bindless testing
void scene_material_set_instance_bindless(scene* scene, material_pass pass_type, struct GLTF_MR_material_resources* resources);
material_2 scene_material_get_instance_bindless(scene* scene, material_id id);
// TEMP: END