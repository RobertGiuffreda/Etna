#include "pipeline.h"

#include "renderer/src/utilities/vkinit.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "platform/filesystem.h"

// TODO: Make computes configurable
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

// TODO: Make computes configurable
void compute_pipeline_destroy(renderer_state* state, compute_pipeline* pipeline) {
    vkDestroyDescriptorSetLayout(state->device.handle, pipeline->descriptor_layout, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, pipeline->layout, state->allocator);
    vkDestroyPipeline(state->device.handle, pipeline->handle, state->allocator);
}

// TODO: Make configurable
b8 graphics_pipeline_create(renderer_state* state, graphics_pipeline_config config, graphics_pipeline* out_pipeline) {
    // Convienience pointers
    shader* vertex = &config.vertex_shader;
    shader* fragment = &config.fragment_shader;

    VkPipelineLayout layout;
    VkPipelineLayoutCreateInfo layout_info = init_pipeline_layout_create_info();
    VK_CHECK(vkCreatePipelineLayout(state->device.handle, &layout_info, state->allocator, &layout));

    ETINFO("Vert Entry Point: %s.", config.vertex_shader.reflection.entry_point);
    ETINFO("Frag Entry Point: %s.", config.fragment_shader.reflection.entry_point);

    // Hardcoding everything for now to get a handle on it
    u32 stage_count = 2;
    VkPipelineShaderStageCreateInfo stages[2] = {
        {   // Vertex Shader
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = 0,
            .stage = vertex->reflection.stage,
            .module = vertex->module,
            .pName = vertex->reflection.entry_point,
            .pSpecializationInfo = 0},
        {   // Fragment Shader
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .pNext = 0,
            .stage = fragment->reflection.stage,
            .module = fragment->module,
            .pName = fragment->reflection.entry_point,
            .pSpecializationInfo = 0}};

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .pNext = 0,
        .primitiveRestartEnable = VK_FALSE,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .pNext = 0,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_CLOCKWISE
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .pNext = 0,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
        .pSampleMask = 0,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    VkPipelineDepthStencilStateCreateInfo depth_stencil = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .pNext = 0,
        .depthTestEnable = VK_FALSE,
        .depthWriteEnable = VK_FALSE,
        .depthCompareOp = VK_COMPARE_OP_NEVER,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {0},
        .back = {0},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    VkFormat color_attachment_format = state->render_image.format;

    VkPipelineRenderingCreateInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .pNext = 0,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &color_attachment_format,
        .depthAttachmentFormat = VK_FORMAT_UNDEFINED
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = 0,

        .viewportCount = 1,
        .scissorCount = 1,
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = 0,
        .logicOpEnable = VK_FALSE,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };
    
    VkDynamicState dynamic_state[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .pNext = 0,
        .dynamicStateCount = 2,
        .pDynamicStates = &dynamic_state[0]
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &render_info,
        .stageCount = stage_count,
        .pStages = stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_stencil,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_info,
        .layout = layout
    };

    VkPipeline new_pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(
        state->device.handle,
        VK_NULL_HANDLE,
        1,
        &pipeline_info,
        state->allocator,
        &new_pipeline));
    
    out_pipeline->descriptor_layout = VK_NULL_HANDLE;
    out_pipeline->descriptor_set = VK_NULL_HANDLE;
    out_pipeline->handle = new_pipeline;
    out_pipeline->layout = layout;
    return true;
}

// TODO: Make configurable
void graphics_pipeline_destroy(renderer_state* state, graphics_pipeline* pipeline) {
    vkDestroyPipeline(state->device.handle, pipeline->handle, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, pipeline->layout, state->allocator);
}