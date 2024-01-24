#include "GLTF_MR.h"

#include "core/logger.h"

#include "renderer/src/shader.h"
#include "renderer/src/descriptor.h"
#include "renderer/src/pipeline.h"
#include "renderer/src/utilities/vkinit.h"

b8 GLTF_MR_build_blueprint(GLTF_MR* mat, renderer_state* state) {
    shader mesh_vert_shader = {0};
    if (!load_shader(state, "build/assets/shaders/mesh_mat.vert.spv", &mesh_vert_shader)) {
        ETERROR("Unable to load mesh_mat.vert.spv");
        return false;
    }

    shader mesh_frag_shader = {0};
    if (!load_shader(state, "build/assets/shaders/mesh_mat.frag.spv", &mesh_frag_shader)) {
        unload_shader(state, &mesh_vert_shader);
        ETERROR("Unable to load mesh_mat.frag.spv");
        return false;
    }

    dsl_builder layout_builder = descriptor_set_layout_builder_create();
    // Scene data binding
    descriptor_set_layout_builder_add_binding(
        &layout_builder,
        /* Binding: */ 0,
        /* Descriptor Count: */ 1,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    
    // Color texture binding
    descriptor_set_layout_builder_add_binding(
        &layout_builder,
        /* Binding: */ 1,
        /* Descriptor Count: */ 1,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    
    // Metal roughness texture binding
    descriptor_set_layout_builder_add_binding(
        &layout_builder,
        /* Binding: */ 2,
        /* Descriptor Count: */ 1,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    
    mat->material_layout = descriptor_set_layout_builder_build(&layout_builder, state);

    descriptor_set_layout_builder_destroy(&layout_builder);

    VkDescriptorSetLayout layouts[] = {
        state->scene_data_descriptor_set_layout,
        mat->material_layout};

    VkPushConstantRange matrix_range;
    matrix_range.offset = 0;
    matrix_range.size = sizeof(gpu_draw_push_constants);
    matrix_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkPipelineLayoutCreateInfo mesh_layout_info = init_pipeline_layout_create_info();
    mesh_layout_info.pushConstantRangeCount = 1;
    mesh_layout_info.pPushConstantRanges = &matrix_range;
    mesh_layout_info.setLayoutCount = 2;
    mesh_layout_info.pSetLayouts = layouts;

    VkPipelineLayout new_layout;
    VK_CHECK(vkCreatePipelineLayout(state->device.handle, &mesh_layout_info, state->allocator, &new_layout));

    mat->opaque_pipeline.layout = new_layout;
    mat->transparent_pipeline.layout = new_layout;

    // build the stage-create-info for both vertex and fragment stages. 
    // This lets the pipeline know the shader modules per stage
    pipeline_builder pipeline_builder = pipeline_builder_create();
    pipeline_builder_set_shaders(&pipeline_builder, mesh_vert_shader, mesh_frag_shader);
    pipeline_builder_set_input_topology(&pipeline_builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
    pipeline_builder_set_polygon_mode(&pipeline_builder, VK_POLYGON_MODE_FILL);
    pipeline_builder_set_cull_mode(&pipeline_builder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
    pipeline_builder_set_multisampling_none(&pipeline_builder);
    pipeline_builder_disable_blending(&pipeline_builder);
    pipeline_builder_enable_depthtest(&pipeline_builder, true, VK_COMPARE_OP_GREATER_OR_EQUAL);

    // render format
    pipeline_builder_set_color_attachment_format(&pipeline_builder, state->render_image.format);
    pipeline_builder_set_depth_attachment_format(&pipeline_builder, state->depth_image.format);

    // Add layout to builder
    pipeline_builder.layout = new_layout;

    // Build opaque variant
    mat->opaque_pipeline.pipeline = pipeline_builder_build(&pipeline_builder, state);

    // Change settings for transparent variant
    pipeline_builder_enable_blending_additive(&pipeline_builder);
    pipeline_builder_enable_depthtest(&pipeline_builder, false, VK_COMPARE_OP_GREATER_OR_EQUAL);

    mat->transparent_pipeline.pipeline = pipeline_builder_build(&pipeline_builder, state);
    pipeline_builder_destroy(&pipeline_builder);

    descriptor_set_writer_initialize(&mat->writer);

    unload_shader(state, &mesh_vert_shader);
    unload_shader(state, &mesh_frag_shader);
}

void GLTF_MR_destroy_blueprint(GLTF_MR* mat, renderer_state* state) {
    // NOTE: GLTF_MR opaque pipeline and transparent pipeline have the 
    // same layout so only one vkDestroyPipelineLayout call is used
    descriptor_set_writer_shutdown(&mat->writer);
    vkDestroyPipeline(state->device.handle, mat->transparent_pipeline.pipeline, state->allocator);
    vkDestroyPipeline(state->device.handle, mat->opaque_pipeline.pipeline, state->allocator);
    vkDestroyPipelineLayout(state->device.handle, mat->opaque_pipeline.layout, state->allocator);
    vkDestroyDescriptorSetLayout(state->device.handle, mat->material_layout, state->allocator);
}

material_instance GLTF_MR_create_instance(
    GLTF_MR* mat,
    renderer_state* state,
    material_pass pass,
    const struct material_resources* resources,
    ds_allocator_growable* descriptor_allocator)
{
    material_instance mat_data;
    mat_data.pass_type = pass;
    if (pass == MATERIAL_PASS_TRANSPARENT) {
        mat_data.pipeline = &mat->transparent_pipeline;
    } else {
        mat_data.pipeline = &mat->opaque_pipeline;
    }

    mat_data.material_set = descriptor_set_allocator_growable_allocate(
        descriptor_allocator, mat->material_layout, state);

    descriptor_set_writer_clear(&mat->writer);
    descriptor_set_writer_write_buffer(
        &mat->writer,
        /* Binding: */ 0,
        resources->data_buffer,
        sizeof(struct material_constants),
        resources->data_buffer_offset,
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
    descriptor_set_writer_write_image(
        &mat->writer,
        /* Binding: */ 1,
        resources->color_image.view,
        resources->color_sampler,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptor_set_writer_write_image(
        &mat->writer,
        /* Binding: */ 2,
        resources->metal_rough_image.view,
        resources->metal_rough_sampler,
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    descriptor_set_writer_update_set(&mat->writer, mat_data.material_set, state);

    return mat_data;
}