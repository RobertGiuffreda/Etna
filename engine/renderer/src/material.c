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
    // TODO: Create load material shaders function that loads shaders and merges the bindings information together
    if (!load_shader(state, vertex_path, &blueprint->vertex)) {
        ETERROR("Unable to load vertex shader %s.", vertex_path);
        return false;
    }

    if (!load_shader(state, fragment_path, &blueprint->fragment)) {
        ETERROR("Unable to load fragment shader %s.", fragment_path);
        unload_shader(state, &blueprint->vertex);
        return false;
    }

    // Begin merging material set binding information from the two shaders

    // Check for presence of designed material set in the shaders
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

    u32 max_binding = 0;
    if (v_has_mat_set) {
        binding_layout* v_bindings = v_sets[v_i].bindings;
        for (u32 i = 0; i < v_sets[v_i].binding_count; ++i) {
            if (v_bindings[i].index > max_binding) max_binding = v_bindings[i].index;
        }
    }

    if (f_has_mat_set) {
        binding_layout* f_bindings = f_sets[f_i].bindings;
        for (u32 i = 0; i < f_sets[f_i].binding_count; ++i) {
            if (f_bindings[i].index > max_binding) max_binding = f_bindings[i].index;
        }
    }

    // Want to index into an array with the binding 
    // number from shader information.
    u32 bindings_array_length = max_binding + 1;

    struct mat_binding_params {
        b8 present;
        u32 count;
        descriptor_type type;
        union {
            u64 buffer_range;
            VkImageLayout image_layout;
        };
        VkShaderStageFlags stage_flags;
    }* binding_parameters;

    binding_parameters = etallocate(sizeof(binding_parameters[0]) * bindings_array_length, MEMORY_TAG_MATERIAL);
    etzero_memory(binding_parameters, sizeof(struct mat_binding_params) * bindings_array_length);

    if (v_has_mat_set) {
        for (u32 i = 0; i < v_sets[v_i].binding_count; ++i) {
            binding_layout binding = v_sets[v_i].bindings[i];
            binding_parameters[binding.index].present = true;
            binding_parameters[binding.index].count = binding.count;
            binding_parameters[binding.index].type = binding.descriptor_type;
            binding_parameters[binding.index].stage_flags |= VK_SHADER_STAGE_VERTEX_BIT;
            switch (binding.descriptor_type)
            {
            case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                binding_parameters[binding.index].image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                binding_parameters[binding.index].buffer_range = binding.block.size;
                break;
            }
        }
    }
    if (f_has_mat_set) {
        for (u32 i = 0; i < f_sets[f_i].binding_count; ++i) {
            binding_layout binding = f_sets[f_i].bindings[i];
            binding_parameters[binding.index].present = true;
            binding_parameters[binding.index].count = binding.count;
            binding_parameters[binding.index].type = binding.descriptor_type;
            binding_parameters[binding.index].stage_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            switch (binding.descriptor_type)
            {
            case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                binding_parameters[binding.index].image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                binding_parameters[binding.index].buffer_range = binding.block.size;
                break;
            }
        }
    }

    // Info for descriptor set writing
    u32 binding_count = 0;
    u32 image_count = 0;
    u32 buffer_count = 0;

    dsl_builder layout_builder = descriptor_set_layout_builder_create();
    for (u32 i = 0; i < bindings_array_length; ++i) {
        if (!binding_parameters[i].present) continue;
        descriptor_set_layout_builder_add_binding(
            &layout_builder,
            /* Binding: */ i,
            binding_parameters[i].count,
            vk_descriptor_type_from_descriptor_type(binding_parameters[i].type),
            binding_parameters[i].stage_flags
        );

        // Info for descriptor set writes
        ++binding_count;
        switch (binding_parameters[i].type)
        {
        case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            image_count += binding_parameters[i].count;
            break;
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            buffer_count += binding_parameters[i].count;
            break;
        }
    }

    blueprint->ds_layout = descriptor_set_layout_builder_build(&layout_builder, state);
    descriptor_set_layout_builder_destroy(&layout_builder);

    VkWriteDescriptorSet* writes = etallocate(sizeof(VkWriteDescriptorSet) * binding_count, MEMORY_TAG_MATERIAL);
    VkDescriptorImageInfo* image_infos = etallocate(sizeof(VkDescriptorImageInfo) * image_count, MEMORY_TAG_MATERIAL);
    VkDescriptorBufferInfo* buffer_infos = etallocate(sizeof(VkDescriptorBufferInfo) * buffer_count, MEMORY_TAG_MATERIAL);
    
    blueprint->binding_count = binding_count;
    blueprint->image_count = image_count;
    blueprint->buffer_count = buffer_count;

    u32 write_index = 0;
    u32 image_info_index = 0;
    u32 buffer_info_index = 0;
    for (u32 i = 0; i < bindings_array_length; ++i) {
        if (!binding_parameters[i].present) continue;
        VkWriteDescriptorSet descriptor_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = 0,
            .dstBinding = i,
            .dstSet = VK_NULL_HANDLE,
            .descriptorCount = binding_parameters[i].count,
            .descriptorType = vk_descriptor_type_from_descriptor_type(binding_parameters[i].type),
            .pImageInfo = image_infos + image_info_index,
            .pBufferInfo = buffer_infos + buffer_info_index,
        };
        writes[write_index++] = descriptor_write;

        switch (binding_parameters[i].type)
        {
        case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            VkDescriptorImageInfo image_info = {
                .imageLayout = binding_parameters[i].image_layout,
            };
            for (u32 j = 0; j < binding_parameters[i].count; ++j) {
                image_infos[j + image_info_index] = image_info;
            }
            image_info_index += binding_parameters[i].count;
            break;
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            VkDescriptorBufferInfo buffer_info = {
                .range = binding_parameters[i].buffer_range,
            };
            for (u32 j = 0; j < binding_parameters[i].count; ++j) {
                buffer_infos[j + buffer_info_index] = buffer_info;
            }
            buffer_info_index += binding_parameters[i].count;
            break;
        }
    }
    blueprint->writes = writes;
    blueprint->image_infos = image_infos;
    blueprint->buffer_infos = buffer_infos;

    etfree(binding_parameters, sizeof(struct mat_binding_params) * bindings_array_length, MEMORY_TAG_MATERIAL);

    b8 v_has_push_constant = blueprint->vertex.push_block_count != 0;
    b8 f_has_push_constant = blueprint->fragment.push_block_count != 0;

    VkPushConstantRange matrix_range = {
        .offset = 0,
        .size = ((v_has_push_constant) ? blueprint->vertex.push_blocks->size :
                 (f_has_push_constant) ? blueprint->fragment.push_blocks->size : 0),
        .stageFlags = (v_has_push_constant && f_has_push_constant) ? VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT :
                      (v_has_push_constant) ? VK_SHADER_STAGE_VERTEX_BIT :
                      (f_has_push_constant) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0,
    };

    VkDescriptorSetLayout ds_layouts[] = {
        state->scene_data_descriptor_set_layout,
        blueprint->ds_layout,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = init_pipeline_layout_create_info();
    pipeline_layout_info.pushConstantRangeCount = (v_has_push_constant || f_has_push_constant) ? 1 : 0;
    pipeline_layout_info.pPushConstantRanges = (v_has_push_constant || f_has_push_constant) ? &matrix_range : NULL;
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

    return true;
}

void material_blueprint_destroy(renderer_state* state, material_blueprint* blueprint) {
    // NOTE: material_blueprint opaque pipeline and transparent pipeline have the 
    // same layout at this point in time so only one vkDestroyPipelineLayout call is used
    vkDestroyPipeline(state->device.handle, blueprint->transparent_pipeline.pipeline, state->allocator);
    vkDestroyPipeline(state->device.handle, blueprint->opaque_pipeline.pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, blueprint->opaque_pipeline.layout, state->allocator);
    vkDestroyDescriptorSetLayout(state->device.handle, blueprint->ds_layout, state->allocator);
    unload_shader(state, &blueprint->fragment);
    unload_shader(state, &blueprint->vertex);

    etfree(blueprint->writes, sizeof(VkWriteDescriptorSet) * blueprint->binding_count, MEMORY_TAG_MATERIAL);
    etfree(blueprint->image_infos, sizeof(VkDescriptorImageInfo) * blueprint->image_count, MEMORY_TAG_MATERIAL);
    etfree(blueprint->buffer_infos, sizeof(VkDescriptorBufferInfo) * blueprint->buffer_count, MEMORY_TAG_MATERIAL);
}

material_instance material_blueprint_create_instance(
    renderer_state* state,
    material_blueprint* blueprint,
    material_pass pass,
    const material_resource* resources,
    ds_allocator* allocator
) {
    material_instance instance;
    instance.pass_type = pass;
    if (pass == MATERIAL_PASS_TRANSPARENT) {
        instance.pipeline = &blueprint->transparent_pipeline;
    } else {
        instance.pipeline = &blueprint->opaque_pipeline;
    }

    instance.material_set = descriptor_set_allocator_allocate(
        allocator, blueprint->ds_layout, state);

    u32 resource_index = 0;
    u32 image_index = 0;
    u32 buffer_index = 0;
    for (u32 i = 0; i < blueprint->binding_count; ++i) {
        blueprint->writes[i].dstSet = instance.material_set;
        u32 j = 0;
        switch (blueprint->writes[i].descriptorType)
        {
        case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            while (j < blueprint->writes[i].descriptorCount) {
                blueprint->image_infos[image_index + j].sampler = resources[resource_index + j].sampler;
                blueprint->image_infos[image_index + j].imageView = resources[resource_index + j].view;
                ++j;
            }
            resource_index += j;
            image_index += j;
            break;
        case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            while (j < blueprint->writes[i].descriptorCount) {
                blueprint->buffer_infos[buffer_index + j].buffer = resources[resource_index + j].buffer;
                blueprint->buffer_infos[buffer_index + j].offset = resources[resource_index + j].offset;
                ++j;
            }
            resource_index += j;
            buffer_index += j;
            break;
        }
    }
    vkUpdateDescriptorSets(
        state->device.handle,
        blueprint->binding_count,
        blueprint->writes,
        /* descriptorCopyCount: */ 0,
        /* pDescriptorCopies: */ 0
    );
    return instance;
}

b8 material_blueprint_create_bindless(renderer_state* state, const char* vertex_path, const char* fragment_path, u32 instance_count, material_blueprint* blueprint) {
    // TODO: Create load material shaders function that loads shaders and merges the bindings information together
    if (!load_shader(state, vertex_path, &blueprint->vertex)) {
        ETERROR("Unable to load vertex shader %s.", vertex_path);
        return false;
    }

    if (!load_shader(state, fragment_path, &blueprint->fragment)) {
        ETERROR("Unable to load fragment shader %s.", fragment_path);
        unload_shader(state, &blueprint->vertex);
        return false;
    }

    // Begin merging material set binding information from the two shaders
    // Check for presence of designed material set in the shaders
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

    u32 max_binding = 0;
    if (v_has_mat_set) {
        binding_layout* v_bindings = v_sets[v_i].bindings;
        for (u32 i = 0; i < v_sets[v_i].binding_count; ++i) {
            if (v_bindings[i].index > max_binding) max_binding = v_bindings[i].index;
        }
    }

    if (f_has_mat_set) {
        binding_layout* f_bindings = f_sets[f_i].bindings;
        for (u32 i = 0; i < f_sets[f_i].binding_count; ++i) {
            if (f_bindings[i].index > max_binding) max_binding = f_bindings[i].index;
        }
    }

    // Want to index into an array with the binding 
    // number from shader information.
    u32 bindings_array_length = max_binding + 1;

    struct mat_binding_params {
        b8 present;
        u32 count;
        descriptor_type type;
        union {
            u64 buffer_range;
            VkImageLayout image_layout;
        };
        VkShaderStageFlags stage_flags;
    }* binding_parameters;

    binding_parameters = etallocate(sizeof(binding_parameters[0]) * bindings_array_length, MEMORY_TAG_MATERIAL);
    etzero_memory(binding_parameters, sizeof(struct mat_binding_params) * bindings_array_length);

    if (v_has_mat_set) {
        for (u32 i = 0; i < v_sets[v_i].binding_count; ++i) {
            binding_layout binding = v_sets[v_i].bindings[i];
            binding_parameters[binding.index].present = true;
            binding_parameters[binding.index].count = binding.count;
            binding_parameters[binding.index].type = binding.descriptor_type;
            binding_parameters[binding.index].stage_flags |= VK_SHADER_STAGE_VERTEX_BIT;
            switch (binding.descriptor_type)
            {
            case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                binding_parameters[binding.index].image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                binding_parameters[binding.index].buffer_range = binding.block.size;
                break;
            }
        }
    }
    if (f_has_mat_set) {
        for (u32 i = 0; i < f_sets[f_i].binding_count; ++i) {
            binding_layout binding = f_sets[f_i].bindings[i];
            binding_parameters[binding.index].present = true;
            binding_parameters[binding.index].count = binding.count;
            binding_parameters[binding.index].type = binding.descriptor_type;
            binding_parameters[binding.index].stage_flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
            switch (binding.descriptor_type)
            {
            case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
                binding_parameters[binding.index].image_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                break;
            case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
                binding_parameters[binding.index].buffer_range = binding.block.size;
                break;
            }
        }
    }

    // Info for descriptor set writing
    u32 binding_count = 0;
    u32 image_count = 0;
    u32 buffer_count = 0;

    dsl_builder layout_builder = descriptor_set_layout_builder_create();
    for (u32 i = 0; i < bindings_array_length; ++i) {
        if (!binding_parameters[i].present) continue;
        descriptor_set_layout_builder_add_binding(
            &layout_builder,
            /* Binding: */ i,
            binding_parameters[i].count,
            vk_descriptor_type_from_descriptor_type(binding_parameters[i].type),
            binding_parameters[i].stage_flags
        );

        // Info for descriptor set writes
        ++binding_count;
        switch (binding_parameters[i].type)
        {
        case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            image_count += binding_parameters[i].count;
            break;
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            buffer_count += binding_parameters[i].count;
            break;
        }
    }

    blueprint->ds_layout = descriptor_set_layout_builder_build(&layout_builder, state);
    descriptor_set_layout_builder_destroy(&layout_builder);

    VkWriteDescriptorSet* writes = etallocate(sizeof(VkWriteDescriptorSet) * binding_count, MEMORY_TAG_MATERIAL);
    VkDescriptorImageInfo* image_infos = etallocate(sizeof(VkDescriptorImageInfo) * image_count, MEMORY_TAG_MATERIAL);
    VkDescriptorBufferInfo* buffer_infos = etallocate(sizeof(VkDescriptorBufferInfo) * buffer_count, MEMORY_TAG_MATERIAL);
    
    blueprint->binding_count = binding_count;
    blueprint->image_count = image_count;
    blueprint->buffer_count = buffer_count;

    u32 write_index = 0;
    u32 image_info_index = 0;
    u32 buffer_info_index = 0;
    for (u32 i = 0; i < bindings_array_length; ++i) {
        if (!binding_parameters[i].present) continue;
        VkWriteDescriptorSet descriptor_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .pNext = 0,
            .dstBinding = i,
            .dstSet = VK_NULL_HANDLE,
            .descriptorCount = binding_parameters[i].count,
            .descriptorType = vk_descriptor_type_from_descriptor_type(binding_parameters[i].type),
            .pImageInfo = image_infos + image_info_index,
            .pBufferInfo = buffer_infos + buffer_info_index,
        };
        writes[write_index++] = descriptor_write;

        switch (binding_parameters[i].type)
        {
        case DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
            VkDescriptorImageInfo image_info = {
                .imageLayout = binding_parameters[i].image_layout,
            };
            for (u32 j = 0; j < binding_parameters[i].count; ++j) {
                image_infos[j + image_info_index] = image_info;
            }
            image_info_index += binding_parameters[i].count;
            break;
        case DESCRIPTOR_TYPE_UNIFORM_BUFFER:
            VkDescriptorBufferInfo buffer_info = {
                .range = binding_parameters[i].buffer_range,
            };
            for (u32 j = 0; j < binding_parameters[i].count; ++j) {
                buffer_infos[j + buffer_info_index] = buffer_info;
            }
            buffer_info_index += binding_parameters[i].count;
            break;
        }
    }
    blueprint->writes = writes;
    blueprint->image_infos = image_infos;
    blueprint->buffer_infos = buffer_infos;

    etfree(binding_parameters, sizeof(struct mat_binding_params) * bindings_array_length, MEMORY_TAG_MATERIAL);

    b8 v_has_push_constant = blueprint->vertex.push_block_count != 0;
    b8 f_has_push_constant = blueprint->fragment.push_block_count != 0;

    VkPushConstantRange matrix_range = {
        .offset = 0,
        .size = ((v_has_push_constant) ? blueprint->vertex.push_blocks->size :
                 (f_has_push_constant) ? blueprint->fragment.push_blocks->size : 0),
        .stageFlags = (v_has_push_constant && f_has_push_constant) ? VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT :
                      (v_has_push_constant) ? VK_SHADER_STAGE_VERTEX_BIT :
                      (f_has_push_constant) ? VK_SHADER_STAGE_FRAGMENT_BIT : 0,
    };

    VkDescriptorSetLayout ds_layouts[] = {
        state->scene_data_descriptor_set_layout,
        blueprint->ds_layout,
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = init_pipeline_layout_create_info();
    pipeline_layout_info.pushConstantRangeCount = (v_has_push_constant || f_has_push_constant) ? 1 : 0;
    pipeline_layout_info.pPushConstantRanges = (v_has_push_constant || f_has_push_constant) ? &matrix_range : NULL;
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

    blueprint->transparent_pipeline.pipeline = pipeline_builder_build(&builder, state);

    pipeline_builder_destroy(&builder);

    return true;
}

material_instance material_blueprint_create_instance_bindless(
    renderer_state* state,
    material_blueprint* blueprint,
    material_pass pass,
    u32 instance_index,
    const material_resource* resources,
    ds_allocator* allocator
) {
    return (material_instance){0};
}

// TODO: Move to a utilities file
VkDescriptorType vk_descriptor_type_from_descriptor_type(descriptor_type type) {
    return (VkDescriptorType)type;
}