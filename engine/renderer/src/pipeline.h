#pragma once

#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"

#define DEFAULT_GRAPHICS_PIPELINE_STAGE_COUNT 2
#define DEFAULT_VERTEX_STAGE_INDEX 0
#define DEFAULT_FRAGMENT_STAGE_INDEX 1

typedef struct pipeline_builder {
    VkPipelineShaderStageCreateInfo stages[DEFAULT_GRAPHICS_PIPELINE_STAGE_COUNT];
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