#include "pipeline.h"

#include "renderer/utilities/vkinit.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "platform/filesystem.h"

// NOTE: This is all very hardcoded for a specific compute shader
b8 compute_pipeline_create(renderer_state* state, compute_pipeline_config config, compute_pipeline* out_pipeline) {
    // Load passed in shader file from config
    if (!filesystem_exists(config.shader)) {
        ETERROR("Unable to find compute shader file: '%s'.", config.shader);
        return false;
    }

    etfile* shader_file = 0;
    if (!filesystem_open(config.shader, FILE_READ_FLAG | FILE_BINARY_FLAG, &shader_file)) {
        ETERROR("Unable to open the compute shader file '%s'.", config.shader);
        return false;
    }

    VkShaderModuleCreateInfo shader_info = init_shader_module_create_info();
    if (!filesystem_size(shader_file, &shader_info.codeSize)) {
        ETERROR("Unable to measure size of compute shader file: '%s'.", config.shader);
        filesystem_close(shader_file);
        return false;
    }

    void* shader_data = etallocate(shader_info.codeSize, MEMORY_TAG_RENDERER);
    if (!filesystem_read_all_bytes(shader_file, shader_data, &shader_info.codeSize)) {
        etfree(shader_data, shader_info.codeSize, MEMORY_TAG_RENDERER);
        filesystem_close(shader_file);
        ETERROR("Error reading bytes from shader code.");
        return false;
    }

    shader_info.pCode = (u32*)shader_data;
    VK_CHECK(vkCreateShaderModule(state->device.handle, &shader_info, state->allocator, &out_pipeline->shader));
    etfree(shader_data, shader_info.codeSize, MEMORY_TAG_RENDERER);
    filesystem_close(shader_file);

    // Create descriptor set layouts for the shader
    VkDescriptorSetLayoutBinding layout_binding = {0};
    layout_binding.binding = 0;
    layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    layout_binding.descriptorCount = 1;
    layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = 
        init_descriptor_set_layout_create_info();
 
    descriptor_set_layout_info.bindingCount = 1;
    descriptor_set_layout_info.pBindings = &layout_binding;

    VK_CHECK(vkCreateDescriptorSetLayout(state->device.handle, &descriptor_set_layout_info, state->allocator, &out_pipeline->descriptor_layout));

    VkDescriptorSetAllocateInfo descriptor_set_alloc = init_descriptor_set_allocate_info();
    descriptor_set_alloc.descriptorPool = state->descriptor_pool;
    descriptor_set_alloc.descriptorSetCount = 1;
    descriptor_set_alloc.pSetLayouts = &out_pipeline->descriptor_layout;

    VK_CHECK(vkAllocateDescriptorSets(state->device.handle, &descriptor_set_alloc, &out_pipeline->descriptor_set));

    VkDescriptorImageInfo descriptor_image_info = {0};
    descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    descriptor_image_info.imageView = state->render_image.view;

    VkWriteDescriptorSet draw_image_write = {0};
	draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	draw_image_write.pNext = 0;

    draw_image_write.dstBinding = 0;
    draw_image_write.dstSet = out_pipeline->descriptor_set;
    draw_image_write.descriptorCount = 1;
    draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    draw_image_write.pImageInfo = &descriptor_image_info;

    vkUpdateDescriptorSets(state->device.handle, 1, &draw_image_write, 0, 0);

    VkPipelineLayoutCreateInfo pipeline_layout_info = init_pipeline_layout_create_info();
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &out_pipeline->descriptor_layout;
    // TEMP: Eventually will not be zero so I am being explicit
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = 0;
    
    VK_CHECK(vkCreatePipelineLayout(state->device.handle, &pipeline_layout_info, state->allocator, &out_pipeline->layout));

    VkPipelineShaderStageCreateInfo pipeline_shader_stage_info = init_pipeline_shader_stage_create_info();
    pipeline_shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    pipeline_shader_stage_info.module = out_pipeline->shader;
    pipeline_shader_stage_info.pName = "main";

    VkComputePipelineCreateInfo comp_pipeline_info = init_compute_pipeline_create_info();
    comp_pipeline_info.layout = out_pipeline->layout;
    comp_pipeline_info.stage = pipeline_shader_stage_info;

    VK_CHECK(vkCreateComputePipelines(state->device.handle, VK_NULL_HANDLE, 1, &comp_pipeline_info, state->allocator, &out_pipeline->handle));
    
    vkDestroyShaderModule(state->device.handle, out_pipeline->shader, state->allocator);
    return true;
}

void compute_pipeline_destroy(renderer_state* state, compute_pipeline* pipeline) {
    vkDestroyDescriptorSetLayout(state->device.handle, pipeline->descriptor_layout, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, pipeline->layout, state->allocator);
    vkDestroyPipeline(state->device.handle, pipeline->handle, state->allocator);
}