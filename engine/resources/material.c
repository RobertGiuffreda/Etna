#include "resources/resource_private.h"
#include "resources/material.h"

#include "core/logger.h"
#include "core/etfile.h"
#include "memory/etmemory.h"

#include "renderer/src/renderer.h"
#include "renderer/src/pipeline.h"
#include "renderer/src/buffer.h"
#include "scene/scene_private.h"

b8 mat_pipe_init(mat_pipe* material, scene* scene, renderer_state* state, const mat_pipe_config* config) {
    // TODO: Create function to load shaders specifically for material shaders & such 
    shader mat_vert;
    if (!load_shader(state, config->vert_path, &mat_vert)) {
        ETERROR("Unable to load shader %s.", config->vert_path);
        return false;
    }

    shader mat_frag;
    if (!load_shader(state, config->frag_path, &mat_frag)) {
        unload_shader(state, &mat_vert);
        ETERROR("Unable to load shader %s.", config->vert_path);
        return false;
    }
    // TODO: END

    // TEMP: Create better pipeline system
    pipeline_builder builder = pipeline_builder_create();
    builder.layout = scene->mat_pipeline_layout;
    pipeline_builder_set_shaders(&builder, mat_vert, mat_frag);
    pipeline_builder_set_input_topology(&builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&builder, VK_POLYGON_MODE_FILL);

    // TODO: Figure out culling for multiple things; on now for some models that use it for
    // the old outline trick (extrude black, invert, cull backfaces)
    pipeline_builder_set_cull_mode(&builder, VK_CULL_MODE_BACK_BIT, VK_FRONT_FACE_COUNTER_CLOCKWISE);
    // TODO: END

    pipeline_builder_set_multisampling_none(&builder);

    // Handle transparency on material pipeline level as a different pipeline object is 
    // required anyway, maybe rework this later
    if (config->transparent) {
        pipeline_builder_enable_blending_additive(&builder);
        pipeline_builder_enable_depthtest(&builder, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
    } else {
        pipeline_builder_disable_blending(&builder);
        pipeline_builder_enable_depthtest(&builder, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
    }

    pipeline_builder_set_color_attachment_format(&builder, state->render_image.format);
    pipeline_builder_set_depth_attachment_format(&builder, state->depth_image.format);
    material->pipe = pipeline_builder_build(&builder, state);
    pipeline_builder_destroy(&builder);

    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE, material->pipe, "MatPipe");

    unload_shader(state, &mat_vert);
    unload_shader(state, &mat_frag);
    // TEMP: END

    buffer_create(
        state,
        sizeof(draw_command) * MAX_DRAW_COMMANDS,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &material->draws_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, material->draws_buffer.handle, "MatDrawsBuffer");
    buffer_create(
        state,
        MAX_MATERIAL_COUNT * config->inst_size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &material->inst_buffer);
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, material->inst_buffer.handle, "MatInstanceDataBuffer");
    VK_CHECK(vkMapMemory(
        state->device.handle,
        material->inst_buffer.memory,
        /* Offset */ 0,
        VK_WHOLE_SIZE,
        /* Flags */ 0,
        &material->inst_data));
    etcopy_memory(material->inst_data, config->instances, config->inst_count * config->inst_size);
    material->inst_count = config->inst_count;
    material->inst_size = config->inst_size;

    // DescriptorSet Stuff
    VkDescriptorSetAllocateInfo allocate_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .pNext = 0,
        .descriptorPool = scene->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &scene->mat_set_layout,
    };
    VK_CHECK(vkAllocateDescriptorSets(
        state->device.handle,
        &allocate_info,
        &material->set));
    SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_SET, material->set, "MatSet");

    VkDescriptorBufferInfo draws_buffer_info = {
        .buffer = material->draws_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet draws_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = material->set,
        .dstBinding = MAT_DRAWS_BINDING,
        .pBufferInfo = &draws_buffer_info,
    };
    VkDescriptorBufferInfo inst_buffer_info = {
        .buffer = material->inst_buffer.handle,
        .offset = 0,
        .range = VK_WHOLE_SIZE,
    };
    VkWriteDescriptorSet inst_buffer_write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .descriptorCount = 1,
        .dstArrayElement = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .dstSet = material->set,
        .dstBinding = MAT_INSTANCES_BINDING,
        .pBufferInfo = &inst_buffer_info,
    };
    VkWriteDescriptorSet writes[] = {
        draws_buffer_write,
        inst_buffer_write,
    };
    vkUpdateDescriptorSets(
        state->device.handle,
        /* writeCount */ 2,
        writes,
        /* copyCount */ 0,
        /* copies */ NULL
    );
    return true;
}

// TODO: Handle destroying the descriptor set handle for the pipeline, currently handled when 
// the scene's descriptor pool is destroyed.
void mat_pipe_shutdown(mat_pipe* material, scene* scene, renderer_state* state) {
    vkUnmapMemory(state->device.handle, material->inst_buffer.memory);
    buffer_destroy(state, &material->inst_buffer);
    buffer_destroy(state, &material->draws_buffer);
    vkDestroyPipeline(state->device.handle, material->pipe, state->allocator);
}

// Linear allocation at the moment
u32 mat_instance_create(mat_pipe* material, renderer_state* state, u64 data_size, void* data) {
    if (material->inst_count >= MAX_MATERIAL_COUNT) {
        ETERROR("Max material instances reached.");
        return INVALID_ID;
    }
    
    u32 inst_index = material->inst_count++;
    u64 inst_data_offset = material->inst_size * inst_index;
    
    etcopy_memory((u8*)material->inst_data + inst_data_offset, data, data_size);
    return inst_index;
}