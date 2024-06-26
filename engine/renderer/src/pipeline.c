#include "pipeline.h"

#include "renderer/src/utilities/vkinit.h"
#include "renderer/src/renderer.h"

#include "data_structures/dynarray.h"

#include "core/logger.h"
#include "memory/etmemory.h"

// TODO: Expand to support multiple color attachments

pipeline_builder pipeline_builder_create(void) {
    pipeline_builder builder = {0};
    for (u32 i = 0; i < DEFAULT_GRAPHICS_PIPELINE_STAGE_COUNT; ++i) {
        builder.stages[i] = init_pipeline_shader_stage_create_info();
    }

    builder.input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    
    builder.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    
    builder.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;

    builder.depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;

    builder.render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    return builder;
}

void pipeline_builder_clear(pipeline_builder* builder) {
    dynarray_destroy(builder->stages);
    
    *builder = pipeline_builder_create();
}

void pipeline_builder_destroy(pipeline_builder* builder) {}

VkPipeline pipeline_builder_build(pipeline_builder* builder, renderer_state* state) {
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
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &builder->color_blend_attachment,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };

    VkPipelineDynamicStateCreateInfo dynamic_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = dynamic_states,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &builder->render_info,
        .stageCount = builder->stage_count,
        .pStages = builder->stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &builder->input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &builder->rasterizer,
        .pMultisampleState = &builder->multisampling,
        .pColorBlendState = &color_blending,
        .pDepthStencilState = &builder->depth_stencil,
        .layout = builder->layout,
        .pDynamicState = &dynamic_info,
    };

    VkPipeline new_pipeline;
    if (vkCreateGraphicsPipelines(
        state->device.handle,
        VK_NULL_HANDLE,
        /* infoCount: */ 1,
        &pipeline_info,
        state->allocator,
        &new_pipeline
    ) != VK_SUCCESS) {
        ETERROR("vkCreategraphicsPipelines failed to create pipeline.");
        return VK_NULL_HANDLE;
    }
    return new_pipeline;
}

// NOTE: Just vertex & fragment shaders supported for now
void pipeline_builder_set_vertex_fragment(pipeline_builder* builder, shader vertex, shader fragment) {
    builder->stages[DEFAULT_VERTEX_STAGE_INDEX].stage = vertex.stage;
    builder->stages[DEFAULT_VERTEX_STAGE_INDEX].module = vertex.module;
    builder->stages[DEFAULT_VERTEX_STAGE_INDEX].pName = vertex.entry_point;

    builder->stages[DEFAULT_FRAGMENT_STAGE_INDEX].stage = fragment.stage;
    builder->stages[DEFAULT_FRAGMENT_STAGE_INDEX].module = fragment.module;
    builder->stages[DEFAULT_FRAGMENT_STAGE_INDEX].pName = fragment.entry_point;

    builder->stage_count = DEFAULT_GRAPHICS_PIPELINE_STAGE_COUNT;
}

void pipeline_builder_set_vertex_only(pipeline_builder* builder, shader vertex) {
    builder->stages[DEFAULT_VERTEX_STAGE_INDEX].stage = vertex.stage;
    builder->stages[DEFAULT_VERTEX_STAGE_INDEX].module = vertex.module;
    builder->stages[DEFAULT_VERTEX_STAGE_INDEX].pName = vertex.entry_point;

    builder->stage_count = 1;
}

void pipeline_builder_set_input_topology(pipeline_builder* builder, VkPrimitiveTopology topology) {
    builder->input_assembly.topology = topology;
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
    // builder->depth_stencil.front = ; Unused at this time
    // builder->depth_stencil.back = ;  Unused at this time
    builder->depth_stencil.minDepthBounds = 0.0f;
    builder->depth_stencil.maxDepthBounds = 1.0f;
}