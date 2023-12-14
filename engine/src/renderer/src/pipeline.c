#include "pipeline.h"

#include "renderer/src/utilities/vkinit.h"

#include "containers/dynarray.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "platform/filesystem.h"

#define DEFAULT_PIPELINE_STAGE_COUNT 2

pipeline_builder pipeline_builder_create(void) {
    pipeline_builder builder = {0};
    builder.stages = dynarray_create(
        DEFAULT_PIPELINE_STAGE_COUNT,
        sizeof(VkPipelineShaderStageCreateInfo));
    
    builder.input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    
    builder.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    
    builder.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    builder.depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    builder.render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    return builder;
}

void pipeline_builder_clear(pipeline_builder* builder) {
    dynarray_destroy(builder->stages);
    builder->stages = 0;
    
    *builder = pipeline_builder_create();
}

void pipeline_builder_destroy(pipeline_builder* builder) {
    dynarray_destroy(builder->stages);
    builder->stages = 0;
}

VkPipeline pipeline_builder_build(pipeline_builder* builder, renderer_state* state) {
    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .pNext = 0,
        .viewportCount = 1,
        .scissorCount = 1
    };

    // setup dummy color blending. We arent using transparent objects yet
    // the blending is just "no blend", but we do write to the color attachment
    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .pNext = 0,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &builder->color_blend_attachment
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkDynamicState dynamic_state[] = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = &dynamic_state[0]
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &builder->render_info,
        .stageCount = dynarray_length(builder->stages),
        .pStages = builder->stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &builder->input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &builder->rasterizer,
        .pMultisampleState = &builder->multisampling,
        .pColorBlendState = &color_blending,
        .pDepthStencilState = &builder->depth_stencil,
        .layout = builder->layout,
        .pDynamicState = &dynamic_info
    };

    VkPipeline new_pipeline;
    if (vkCreateGraphicsPipelines(
        state->device.handle,
        VK_NULL_HANDLE,
        1, &pipeline_info,
        state->allocator,
        &new_pipeline) != VK_SUCCESS)
    {
        ETERROR("vkCreategraphicsPipelines failed to create pipeline");
        return VK_NULL_HANDLE;
    }
    return new_pipeline;
}

void pipeline_builder_set_shaders(pipeline_builder* builder, shader vertex, shader fragment) {
    // TODO: Set the pipeline layout information in here by creating descriptor set layout from shaders

    // TODO: Loop & input an array & count or dynarray
    VkPipelineShaderStageCreateInfo vertex_stage = init_pipeline_shader_stage_create_info();
    vertex_stage.stage = vertex.stage;
    vertex_stage.module = vertex.module;
    vertex_stage.pName = vertex.entry_point;

    VkPipelineShaderStageCreateInfo fragment_stage = init_pipeline_shader_stage_create_info();
    fragment_stage.stage = fragment.stage;
    fragment_stage.module = fragment.module;
    fragment_stage.pName = fragment.entry_point;

    dynarray_push((void**)&builder->stages, &vertex_stage);
    dynarray_push((void**)&builder->stages, &fragment_stage);
}

void pipeline_builder_set_input_topology(pipeline_builder* builder, VkPrimitiveTopology topology) {
    builder->input_assembly.topology = topology;
    // TODO: Look into primitive restart & see use cases
    builder->input_assembly.primitiveRestartEnable = VK_FALSE;
}

void pipeline_builder_set_polygon_mode(pipeline_builder* builder, VkPolygonMode mode) {
    builder->rasterizer.polygonMode = mode;
    builder->rasterizer.lineWidth = 1.0f;
}

void pipeline_builder_set_cull_mode(pipeline_builder* builder, VkCullModeFlags cull_mode, VkFrontFace front_face) {
    builder->rasterizer.cullMode = cull_mode;
    builder->rasterizer.frontFace = front_face;
}

void pipeline_builder_set_multisampling_none(pipeline_builder* builder) {
    VkPipelineMultisampleStateCreateInfo* multisampling = &builder->multisampling;
    multisampling->sampleShadingEnable = VK_FALSE;
    multisampling->rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling->minSampleShading = 1.0f;
    multisampling->pSampleMask = 0;
    multisampling->alphaToCoverageEnable = VK_FALSE;
    multisampling->alphaToOneEnable = VK_FALSE;
}

void pipeline_builder_disable_blending(pipeline_builder* builder) {
    builder->color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                     VK_COLOR_COMPONENT_G_BIT |
                                                     VK_COLOR_COMPONENT_B_BIT |
                                                     VK_COLOR_COMPONENT_A_BIT;
    builder->color_blend_attachment.blendEnable = VK_FALSE;
}

void pipeline_builder_enable_blending_additive(pipeline_builder* builder) {
    builder->color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                     VK_COLOR_COMPONENT_G_BIT |
                                                     VK_COLOR_COMPONENT_B_BIT |
                                                     VK_COLOR_COMPONENT_A_BIT;
    builder->color_blend_attachment.blendEnable = VK_TRUE;
    builder->color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    builder->color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    builder->color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    builder->color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    builder->color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    builder->color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void pipeline_builder_enable_blending_alphablend(pipeline_builder* builder) {
    builder->color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
                                                     VK_COLOR_COMPONENT_G_BIT |
                                                     VK_COLOR_COMPONENT_B_BIT |
                                                     VK_COLOR_COMPONENT_A_BIT;
    builder->color_blend_attachment.blendEnable = VK_TRUE;
    builder->color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA;
    builder->color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    builder->color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    builder->color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    builder->color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    builder->color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

// TODO: Expand to support multiple color attachments
void pipeline_builder_set_color_attachment_format(pipeline_builder* builder, VkFormat format) {
    builder->color_attachment_format = format;
    builder->render_info.colorAttachmentCount = 1;
    builder->render_info.pColorAttachmentFormats = &builder->color_attachment_format;
}

void pipeline_builder_set_depth_format(pipeline_builder* builder, VkFormat format) {
    builder->render_info.depthAttachmentFormat = format;
}

void pipeline_builder_disable_depthtest(pipeline_builder* builder) {
    builder->depth_stencil.depthTestEnable = VK_FALSE;
    builder->depth_stencil.depthWriteEnable = VK_FALSE;
    builder->depth_stencil.depthCompareOp = VK_COMPARE_OP_NEVER;
    builder->depth_stencil.depthBoundsTestEnable = VK_FALSE;
    // builder->depth_stencil.front is defaulted to zero
    // builder->depth_stencil.back is defaulted to zero
    builder->depth_stencil.minDepthBounds = 0.0f;
    builder->depth_stencil.maxDepthBounds = 1.0f;
}

void pipeline_builder_enable_depthtest(pipeline_builder* builder, b8 depth_write_enable, VkCompareOp op) {
    builder->depth_stencil.depthTestEnable = VK_TRUE;
    builder->depth_stencil.depthWriteEnable = depth_write_enable;
    builder->depth_stencil.depthCompareOp = op;
    builder->depth_stencil.depthBoundsTestEnable = VK_FALSE;
    // builder->depth_stencil.front is defaulted to zero
    // builder->depth_stencil.back is defaulted to zero
    builder->depth_stencil.minDepthBounds = 0.0f;
    builder->depth_stencil.maxDepthBounds = 1.0f;
}

// // TODO: Make computes configurable
// b8 compute_pipeline_create(renderer_state* state, compute_pipeline_config config, compute_pipeline* out_pipeline) {
//     // Load passed in shader file from config
//     if (!filesystem_exists(config.shader)) {
//         ETERROR("Unable to find compute shader file: '%s'.", config.shader);
//         return false;
//     }

//     etfile* shader_file = 0;
//     if (!filesystem_open(config.shader, FILE_READ_FLAG | FILE_BINARY_FLAG, &shader_file)) {
//         ETERROR("Unable to open the compute shader file '%s'.", config.shader);
//         return false;
//     }

//     VkShaderModuleCreateInfo shader_info = init_shader_module_create_info();
//     if (!filesystem_size(shader_file, &shader_info.codeSize)) {
//         ETERROR("Unable to measure size of compute shader file: '%s'.", config.shader);
//         filesystem_close(shader_file);
//         return false;
//     }

//     void* shader_data = etallocate(shader_info.codeSize, MEMORY_TAG_RENDERER);
//     if (!filesystem_read_all_bytes(shader_file, shader_data, &shader_info.codeSize)) {
//         etfree(shader_data, shader_info.codeSize, MEMORY_TAG_RENDERER);
//         filesystem_close(shader_file);
//         ETERROR("Error reading bytes from shader code.");
//         return false;
//     }

//     shader_info.pCode = (u32*)shader_data;
//     VK_CHECK(vkCreateShaderModule(state->device.handle, &shader_info, state->allocator, &out_pipeline->shader));
//     etfree(shader_data, shader_info.codeSize, MEMORY_TAG_RENDERER);
//     filesystem_close(shader_file);

//     // Create descriptor set layouts for the compute effect
//     VkDescriptorSetLayoutBinding layout_binding = {0};
//     layout_binding.binding = 0;
//     layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//     layout_binding.descriptorCount = 1;
//     layout_binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

//     VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = 
//         init_descriptor_set_layout_create_info();
 
//     descriptor_set_layout_info.bindingCount = 1;
//     descriptor_set_layout_info.pBindings = &layout_binding;

//     VK_CHECK(vkCreateDescriptorSetLayout(state->device.handle, &descriptor_set_layout_info, state->allocator, &out_pipeline->descriptor_layout));

//     VkDescriptorSetAllocateInfo descriptor_set_alloc = init_descriptor_set_allocate_info();
//     descriptor_set_alloc.descriptorPool = state->descriptor_pool;
//     descriptor_set_alloc.descriptorSetCount = 1;
//     descriptor_set_alloc.pSetLayouts = &out_pipeline->descriptor_layout;

//     VK_CHECK(vkAllocateDescriptorSets(state->device.handle, &descriptor_set_alloc, &out_pipeline->descriptor_set));

//     VkDescriptorImageInfo descriptor_image_info = {0};
//     descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
//     descriptor_image_info.imageView = state->render_image.view;

//     VkWriteDescriptorSet draw_image_write = {0};
// 	draw_image_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
// 	draw_image_write.pNext = 0;

//     draw_image_write.dstBinding = 0;
//     draw_image_write.dstSet = out_pipeline->descriptor_set;
//     draw_image_write.descriptorCount = 1;
//     draw_image_write.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//     draw_image_write.pImageInfo = &descriptor_image_info;

//     vkUpdateDescriptorSets(state->device.handle, 1, &draw_image_write, 0, 0);

//     VkPipelineLayoutCreateInfo pipeline_layout_info = init_pipeline_layout_create_info();
//     pipeline_layout_info.setLayoutCount = 1;
//     pipeline_layout_info.pSetLayouts = &out_pipeline->descriptor_layout;
//     // TEMP: Eventually will not be zero so I am being explicit
//     pipeline_layout_info.pushConstantRangeCount = 0;
//     pipeline_layout_info.pPushConstantRanges = 0;
    
//     VK_CHECK(vkCreatePipelineLayout(state->device.handle, &pipeline_layout_info, state->allocator, &out_pipeline->layout));

//     VkPipelineShaderStageCreateInfo pipeline_shader_stage_info = init_pipeline_shader_stage_create_info();
//     pipeline_shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
//     pipeline_shader_stage_info.module = out_pipeline->shader;
//     pipeline_shader_stage_info.pName = "main";

//     VkComputePipelineCreateInfo comp_pipeline_info = init_compute_pipeline_create_info();
//     comp_pipeline_info.layout = out_pipeline->layout;
//     comp_pipeline_info.stage = pipeline_shader_stage_info;

//     VK_CHECK(vkCreateComputePipelines(state->device.handle, VK_NULL_HANDLE, 1, &comp_pipeline_info, state->allocator, &out_pipeline->handle));
    
//     vkDestroyShaderModule(state->device.handle, out_pipeline->shader, state->allocator);
//     return true;
// }

// // TODO: Make computes configurable
// void compute_pipeline_destroy(renderer_state* state, compute_pipeline* pipeline) {
//     vkDestroyDescriptorSetLayout(state->device.handle, pipeline->descriptor_layout, state->allocator);
//     vkDestroyPipelineLayout(state->device.handle, pipeline->layout, state->allocator);
//     vkDestroyPipeline(state->device.handle, pipeline->handle, state->allocator);
// }