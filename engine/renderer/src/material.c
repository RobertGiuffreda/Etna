#include "material.h"

#include "memory/etmemory.h"
#include "core/logger.h"

#include "renderer/src/renderer.h"
#include "renderer/src/descriptor.h"
#include "renderer/src/pipeline.h"    
#include "renderer/src/utilities/vkinit.h"

#define MATERIAL_SET_INDEX 1

VkDescriptorType vk_descriptor_type_from_descriptor_type(descriptor_type type);

b8 material_blueprint_create(renderer_state* state, const char* vertex_path, const char* fragment_path, material_blueprint* blueprint) {
    if (!load_shader(state, vertex_path, &blueprint->vertex)) {
        ETERROR("Unable to load vertex shader %s.", vertex_path);
        return false;
    }

    if (!load_shader(state, fragment_path, &blueprint->fragment)) {
        ETERROR("Unable to load fragment shader %s.", fragment_path);
        unload_shader(state, &blueprint->vertex);
        return false;
    }

    // Find the material set within the shaders
    set_layout* v_sets = blueprint->vertex.sets;
    u32 v_set_count = blueprint->vertex.set_count;
    u32 v_i = 0;
    while (v_i < v_set_count && v_sets[v_i].index != MATERIAL_SET_INDEX) ++v_i;
    b8 v_has_mat_set = (v_i != v_set_count);

    set_layout* f_sets = blueprint->fragment.sets;
    u32 f_set_count = blueprint->fragment.set_count;
    u32 f_i = 0;    
    while (f_i < f_set_count && f_sets[f_i].index != MATERIAL_SET_INDEX) ++f_i;
    b8 f_has_mat_set = (f_i != f_set_count);

    u32 v_max_binding = 0;
    if (v_has_mat_set) {
        binding_layout* v_bindings = v_sets[v_i].bindings;
        for (u32 i = 0; i < v_sets[v_i].binding_count; ++i) {
            if (v_bindings[i].index > v_max_binding) v_max_binding = v_bindings[i].index;
        }
    }

    u32 f_max_binding = 0;
    if (f_has_mat_set) {
        binding_layout* f_bindings = f_sets[f_i].bindings;
        for (u32 i = 0; i < f_sets[f_i].binding_count; ++i) {
            if (f_bindings[i].index > f_max_binding) f_max_binding = f_bindings[i].index;
        }
    }

    u32 binding_count = ((v_max_binding > f_max_binding) ? v_max_binding : f_max_binding) + 1;

    struct {
        b8 present;
        u32 binding;
        u32 count;
        VkDescriptorType type;
        VkShaderStageFlags stage_flags;
    }*binding_parameters;

    binding_parameters = etallocate(sizeof(binding_parameters[0]) * binding_count, MEMORY_TAG_MATERIAL);
    etzero_memory(binding_parameters, sizeof(binding_parameters[0]) * binding_count);

    // NOTE: v_sets[v_i].bindings[i] & f_sets[f_i].bindings[i] is odd
    for (u32 i = 0; i < binding_count; ++i) {
        if (v_has_mat_set && i < v_sets[v_i].binding_count) {
            binding_layout* binding = &v_sets[v_i].bindings[i];
            binding_parameters[binding->index].present = true;
            binding_parameters[binding->index].count = binding->count;
            binding_parameters[binding->index].binding = binding->index;
            binding_parameters[binding->index].type = vk_descriptor_type_from_descriptor_type(binding->descriptor_type);
            binding_parameters[binding->index].stage_flags |= VK_SHADER_STAGE_VERTEX_BIT;
        }
        if (f_has_mat_set && i < f_sets[f_i].binding_count) {
            binding_layout* binding = &f_sets[f_i].bindings[i];
            binding_parameters[binding->index].present = true;
            binding_parameters[binding->index].count = binding->count;
            binding_parameters[binding->index].binding = binding->index;
            binding_parameters[binding->index].type = vk_descriptor_type_from_descriptor_type(binding->descriptor_type);
            binding_parameters[binding->index].stage_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
    }

    // Create descriptor set layout from binding parameters
    dsl_builder layout_builder = descriptor_set_layout_builder_create();
    for (u32 i = 0; i < binding_count; ++i) {
        // If the descriptor index is not present in the layouts do not bind it
        if (!binding_parameters[i].present) continue;
        descriptor_set_layout_builder_add_binding(
            &layout_builder,
            binding_parameters[i].binding,
            binding_parameters[i].count,
            binding_parameters[i].type,
            binding_parameters[i].stage_flags);
    }

    blueprint->ds_layout = descriptor_set_layout_builder_build(&layout_builder, state);
    descriptor_set_layout_builder_destroy(&layout_builder);
    etfree(binding_parameters, sizeof(binding_parameters[0]) * binding_count, MEMORY_TAG_MATERIAL);

    VkDescriptorSetLayout ds_layouts[] = {
        state->scene_data_descriptor_set_layout,
        blueprint->ds_layout};

    // NOTE: Push constants are used by the engine to send the 
    // vertex buffer address and model matrix for each draw call
    // So its hard coded 
    VkPushConstantRange matrix_range;
    matrix_range.offset = 0;
    matrix_range.size = sizeof(gpu_draw_push_constants);
    matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo pipeline_layout_info = init_pipeline_layout_create_info();
    pipeline_layout_info.pushConstantRangeCount = 1;
    pipeline_layout_info.pPushConstantRanges = &matrix_range;
    pipeline_layout_info.setLayoutCount = 2;
    pipeline_layout_info.pSetLayouts = ds_layouts;

    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(state->device.handle, &pipeline_layout_info, state->allocator, &pipeline_layout));

    blueprint->opaque_pipeline.layout = pipeline_layout;
    blueprint->transparent_pipeline.layout = pipeline_layout;

    pipeline_builder builder = pipeline_builder_create();
    pipeline_builder_set_shaders(&builder, blueprint->vertex, blueprint->fragment);
    pipeline_builder_set_input_topology(&builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&builder, VK_POLYGON_MODE_FILL);
    pipeline_builder_set_cull_mode(&builder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder_set_multisampling_none(&builder);
    pipeline_builder_disable_blending(&builder);
    pipeline_builder_enable_depthtest(&builder, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipeline_builder_set_color_attachment_format(&builder, state->render_image.format);
    pipeline_builder_set_depth_attachment_format(&builder, state->depth_image.format);

    builder.layout = pipeline_layout;

    blueprint->opaque_pipeline.pipeline = pipeline_builder_build(&builder, state);

    // Change pipeline builder settings to create transparent pipeline version
    pipeline_builder_enable_blending_additive(&builder);
    pipeline_builder_enable_depthtest(&builder, false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    pipeline_builder_destroy(&builder);

    descriptor_set_writer_initialize(&blueprint->writer);

    return true;
}

void material_blueprint_destroy(renderer_state* state, material_blueprint* blueprint) {
    // NOTE: material_blueprint opaque pipeline and transparent pipeline have the 
    // same layout at this point in time so only one vkDestroyPipelineLayout call is used
    descriptor_set_writer_shutdown(&blueprint->writer);
    vkDestroyPipeline(state->device.handle, blueprint->transparent_pipeline.pipeline, state->allocator);
    vkDestroyPipeline(state->device.handle, blueprint->opaque_pipeline.pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, blueprint->opaque_pipeline.layout, state->allocator);
    vkDestroyDescriptorSetLayout(state->device.handle, blueprint->ds_layout, state->allocator);
    unload_shader(state, &blueprint->fragment);
    unload_shader(state, &blueprint->vertex);
}

material_instance material_blueprint_create_instance(
    renderer_state* state,
    material_blueprint* blueprint,
    material_pass pass,
    const struct material_resources* resources,
    ds_allocator* allocator)
{
    material_instance instance;
    instance.pass_type = pass;
    if (pass == MATERIAL_PASS_TRANSPARENT) {
        instance.pipeline = &blueprint->transparent_pipeline;
    } else {
        instance.pipeline = &blueprint->opaque_pipeline;
    }

    instance.material_set = descriptor_set_allocator_allocate(
        allocator, blueprint->ds_layout, state);

    descriptor_set_writer_clear(&blueprint->writer);

    descriptor_set_writer_write_buffer(
        &blueprint->writer,
        /* Binding: */ 0,
        resources->data_buffer,
        sizeof(struct material_constants),
        resources->data_buffer_offset,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptor_set_writer_write_image(
        &blueprint->writer,
        /* Binding: */ 1,
        resources->color_image.view,
        resources->color_sampler,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptor_set_writer_write_image(
        &blueprint->writer,
        /* Binding: */ 2,
        resources->metal_rough_image.view,
        resources->metal_rough_sampler,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptor_set_writer_update_set(&blueprint->writer, instance.material_set, state);

    return instance;
}

// TODO: Move to a utilities file
VkDescriptorType vk_descriptor_type_from_descriptor_type(descriptor_type type) {
    return (VkDescriptorType)type;
}