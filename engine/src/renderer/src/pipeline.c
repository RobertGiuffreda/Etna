#include "pipeline.h"

#include "renderer/src/utilities/vkinit.h"

#include "containers/dynarray.h"

#include "core/etmemory.h"
#include "core/logger.h"

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

void pipeline_builder_set_shaders(pipeline_builder* builder, shader1 vertex, shader1 fragment) {
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
    builder->color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    builder->color_blend_attachment.blendEnable = VK_FALSE;
}

void pipeline_builder_enable_blending_additive(pipeline_builder* builder) {
    builder->color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    builder->color_blend_attachment.blendEnable = VK_TRUE;
    builder->color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    builder->color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA;
    builder->color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
    builder->color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    builder->color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    builder->color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
}

void pipeline_builder_enable_blending_alphablend(pipeline_builder* builder) {
    builder->color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
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

void pipeline_builder_set_depth_attachment_format(pipeline_builder* builder, VkFormat format) {
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
    // builder->depth_stencil.front = ;
    // builder->depth_stencil.back = ;
    builder->depth_stencil.minDepthBounds = 0.0f;
    builder->depth_stencil.maxDepthBounds = 1.0f;
}