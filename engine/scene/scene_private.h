#pragma once
#include "defines.h"

#include "core/camera.h"
#include "math/math_types.h"
#include "renderer/src/vk_types.h"
#include "renderer/src/renderables.h"

#include "resources/resource_private.h"

/** Checklist:
 * Bind opaque pipeline and test
 * Do non indirect version
 * 
 * Indirect:
 * Scene buffers
 * Draw buffers
 * 
 * Indirect generated on GPU
 * 
 */

typedef struct scene {
    char* name;

    // TEMP: Until bindless has taken over
    u64 vertex_count;
    vertex* vertices;
    u64 index_count;
    u32* indices;

    buffer index_buffer;
    buffer vertex_buffer;
    VkDeviceAddress vb_addr;    // Vertex buffer address
    buffer transform_buffer;
    VkDeviceAddress tb_addr;    // Transform buffer address

    // HACK: Create on fly just to see if shaders work??
    buffer* draws_buffer;    // Perframe
    buffer* scene_uniforms;     // Perframe
    // HACK: END

    buffer bindless_material_buffer;

    surface_2* surfaces;    // dynarray
    mesh_2* meshes;         // dynarray
    m4s* transforms;        // dynarray
    object* objects;        // dynarray

    u64 surface_count;
    u64 mesh_count;
    u64 transform_count;
    u64 object_count;

    // Bindless & indirect descriptor set layout is different so this is here until bindless takeover
    VkDescriptorSetLayout scene_layout;
    VkDescriptorSet* scene_sets;    // Per frame

    VkPushConstantRange push_constant;

    // Material blueprint(one uber shader) information for bindless instances:
    VkPipeline opaque_pipeline;
    VkPipeline transparent_pipeline;
    VkPipelineLayout pipeline_layout;

    VkDescriptorSetLayout material_layout;
    VkDescriptorSet material_set;

    material_2 materials[MAX_MATERIAL_COUNT];
    u32 material_count;
    
    // TEMP: Hardcoded for now, rework when materials get abstraction back
    VkDescriptorPool bindless_pool;
    // TEMP: END

    // Draw commands for indirect bindless.
    u64 op_draws_count;
    u64 tp_draws_count;

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
    renderer_state* state;
} scene;

// TEMP: Bindless testing. Jank
b8 scene_material_set_instance_bindless(scene* scene, material_pass pass_type, struct bindless_material_resources* resources);
material_2 scene_material_get_instance_bindless(scene* scene, material_id id);

void scene_image_set_bindless(scene* scene, u32 img_id, u32 sampler_id);
// TEMP: END