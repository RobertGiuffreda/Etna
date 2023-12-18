#pragma once

#include "renderer/src/vk_types.h"

// TODO: Move from vkguide.dev to own architecture decisions and structs
// TODO: Options to create from shaders & another pipeline_builder object
typedef struct pipeline_builder {
    // Dynarray
    VkPipelineShaderStageCreateInfo* stages;
    VkPipelineInputAssemblyStateCreateInfo input_assembly;
    VkPipelineRasterizationStateCreateInfo rasterizer;
    VkPipelineColorBlendAttachmentState color_blend_attachment;
    VkPipelineLayout layout;
    VkPipelineMultisampleStateCreateInfo multisampling;
    VkPipelineDepthStencilStateCreateInfo depth_stencil;
    VkPipelineRenderingCreateInfo render_info;
    VkFormat color_attachment_format;
} pipeline_builder;

pipeline_builder pipeline_builder_create(void);

void pipeline_builder_destroy(pipeline_builder* builder);

VkPipeline pipeline_builder_build(pipeline_builder* builder, renderer_state* state);

void pipeline_builder_set_shaders(pipeline_builder* builder, shader vertex, shader fragment);

void pipeline_builder_set_input_topology(pipeline_builder* builder, VkPrimitiveTopology topology);

void pipeline_builder_set_polygon_mode(pipeline_builder* builder, VkPolygonMode mode);

void pipeline_builder_set_cull_mode(pipeline_builder* builder, VkCullModeFlags cull_mode, VkFrontFace front_face);

void pipeline_builder_set_multisampling_none(pipeline_builder* builder);

void pipeline_builder_disable_blending(pipeline_builder* builder);

void pipeline_builder_enable_blending_additive(pipeline_builder* builder);

void pipeline_builder_enable_blending_alphablend(pipeline_builder* builder);

void pipeline_builder_set_color_attachment_format(pipeline_builder* builder, VkFormat format);

void pipeline_builder_set_depth_attachment_format(pipeline_builder* builder, VkFormat format);

void pipeline_builder_disable_depthtest(pipeline_builder* builder);

void pipeline_builder_enable_depthtest(pipeline_builder* builder, b8 depth_write_enable, VkCompareOp op);