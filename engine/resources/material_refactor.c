// #include "resources/resource_private.h"
// #include "core/logger.h"
// #include "core/etfile.h"
// #include "memory/etmemory.h"
// #include "renderer/src/pipeline.h"
// #include "renderer/src/buffer.h"

// /** TODO: Replace hardcoding with reflection data
//  * Load shaders
//  * 
//  * Init takes scene instead of renderer_state.
//  */
// b8 load_mat_shader(
//     const char* vert_path,
//     const char* frag_path,
//     renderer_state* state,
//     mat_shader* shader
// ) {
//     return false;
// }

// typedef enum MAT_SET_BINDINGS {
//     MAT_DRAW_BUFFER_BINDING = 0,
//     MAT_INSTANCE_UNIFORM_ARRAY,
//     MAT_MAX_BINDING,
// } MAT_SET_BINDINGS;

// b8 mat_init(mat_config* config, mat* material) {
//     renderer_state* state = config->state;
//     scene* scene = config->scene;

//     // TODO: Replace with load mat shader function
//     shader mat_vert;
//     if (!load_shader(state, config->vert_path, &mat_vert)) {
//         ETERROR("Unable to load shader %s.", config->vert_path);
//         return false;
//     }

//     shader mat_frag;
//     if (!load_shader(state, config->frag_path, &mat_frag)) {
//         unload_shader(state, &mat_vert);
//         ETERROR("Unable to load shader %s.", config->vert_path);
//         return false;
//     }
//     // TODO: END

//     VkDescriptorSetLayoutBinding bindings[] = {
//         [MAT_DRAW_BUFFER_BINDING] = {
//             .binding = MAT_DRAW_BUFFER_BINDING,
//             .descriptorCount = 1,
//             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
//             .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
//             .pImmutableSamplers = NULL,
//         },
//         [MAT_INSTANCE_UNIFORM_ARRAY] = {
//             .binding = MAT_INSTANCE_UNIFORM_ARRAY,
//             .descriptorCount = MAX_MATERIAL_COUNT,
//             .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
//             .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
//             .pImmutableSamplers = NULL,
//         },
//     };
//     VkDescriptorBindingFlags common_flags = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
//                                             VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT |
//                                             VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
//     VkDescriptorBindingFlags bindings_flags[] = {
//         [MAT_DRAW_BUFFER_BINDING] = common_flags,
//         [MAT_INSTANCE_UNIFORM_ARRAY] = common_flags | VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT,
//     };
//     VkDescriptorSetLayoutBindingFlagsCreateInfo bindings_flags = {
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
//         .pNext = 0,
//         .bindingCount = MAT_MAX_BINDING,
//         .pBindingFlags = bindings_flags,
//     };
//     VkDescriptorSetLayoutCreateInfo layout_info = {
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
//         .pNext = &bindings_flags,
//         .bindingCount = MAT_MAX_BINDING,
//         .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT,
//         .pBindings = bindings,
//     };
//     VK_CHECK(vkCreateDescriptorSetLayout(
//         state->device.handle,
//         &layout_info,
//         state->allocator,
//         &material->set_layout));
//     SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT, material->set_layout, "MatSetLayout");

//     u32 mat_set_variable_uniform_count = MAX_MATERIAL_COUNT;
//     VkDescriptorSetVariableDescriptorCountAllocateInfo mat_set_variable_descriptor_count_info = {
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
//         .descriptorSetCount = 1,
//         .pDescriptorCounts = &mat_set_variable_uniform_count,
//     };
//     VkDescriptorSetAllocateInfo allocate_info = {
//         .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
//         .pNext = &mat_set_variable_descriptor_count_info,
//         .descriptorPool = scene->descriptor_pool,
//         .descriptorSetCount = 1,
//         .pSetLayouts = &material->set_layout,
//     };
//     VK_CHECK(vkAllocateDescriptorSets(
//         state->device.handle,
//         &allocate_info,
//         &material->pipe.set));
//     SET_DEBUG_NAME(state, VK_OBJECT_TYPE_DESCRIPTOR_SET, material->pipe.set, "MatSet");

//     VkDescriptorSetLayout set_layouts[] = {
//         [0] = scene->scene_set_layout,
//         [1] = material->set_layout,
//     };
//     VkPipelineLayoutCreateInfo pipe_layout_info = {
//         .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
//         .setLayoutCount = 2,
//         .pSetLayouts = set_layouts,
//     };
//     VK_CHECK(vkCreatePipelineLayout(
//         state->device.handle,
//         &layout_info,
//         state->allocator,
//         &material->pipe.layout));
//     SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE_LAYOUT, material->pipe.layout, "MatPipeLayout");

//     // TEMP: Create material pipeline builder
//     pipeline_builder builder = pipeline_builder_create();
//     builder.layout = material->pipe.layout;
//     pipeline_builder_set_shaders(&builder, mat_vert, mat_frag);
//     pipeline_builder_set_input_topology(&builder, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
//     pipeline_builder_set_polygon_mode(&builder, VK_POLYGON_MODE_FILL);
//     pipeline_builder_set_cull_mode(&builder, VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
//     pipeline_builder_set_multisampling_none(&builder);

//     // Handle transparency on material pipeline level as a different pipeline object is 
//     // required anyway, maybe rework this later
//     if (config->transparent) {
//         pipeline_builder_enable_blending_additive(&builder);
//         pipeline_builder_enable_depthtest(&builder, false, VK_COMPARE_OP_GREATER_OR_EQUAL);
//     } else {
//         pipeline_builder_disable_blending(&builder);
//         pipeline_builder_enable_depthtest(&builder, true, VK_COMPARE_OP_GREATER_OR_EQUAL);
//     }

//     pipeline_builder_set_color_attachment_format(&builder, state->render_image.format);
//     pipeline_builder_set_depth_attachment_format(&builder, state->depth_image.format);
//     material->pipe.pipeline = pipeline_builder_build(&builder, state);
//     pipeline_builder_destroy(&builder);

//     SET_DEBUG_NAME(state, VK_OBJECT_TYPE_PIPELINE, material->pipe.pipeline, "MatPipe");

//     unload_shader(state, &mat_vert);
//     unload_shader(state, &mat_frag);
//     // TEMP: END

//     // Aligned size
//     u64 min_ubo_offset_alignment = state->device.gpu_limits.minUniformBufferOffsetAlignment;
//     u64 inst_data_offset_alignment = config->inst_data_size;
//     if (min_ubo_offset_alignment) {
//         inst_data_offset_alignment = inst_data_offset_alignment + min_ubo_offset_alignment - 1;
//         inst_data_offset_alignment &= ~(min_ubo_offset_alignment - 1);
//     }
//     material->inst_block_size = inst_data_offset_alignment;

//     buffer_create(
//         state,
//         MAX_MATERIAL_COUNT * material->inst_block_size,
//         VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
//         VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
//         &material->inst_data_buffer);
//     SET_DEBUG_NAME(state, VK_OBJECT_TYPE_BUFFER, material->inst_data_buffer.handle, "MatInstanceDataBuffer");
//     VK_CHECK(vkMapMemory(
//         state->device.handle,
//         material->inst_data_buffer.memory,
//         /* Offset */ 0,
//         VK_WHOLE_SIZE,
//         /* Flags */ 0,
//         &material->inst_data
//     ));
//     return true;
// }

// // TODO: Handle destroying the descriptor set handle for the pipeline, currently handled when 
// // scenes descriptor pool is destroyed
// void mat_shutdown(mat* material, renderer_state* state) {
//     vkUnmapMemory(state->device.handle, material->inst_data_buffer.memory);
//     buffer_destroy(state, &material->inst_data_buffer);
//     vkDestroyPipeline(state->device.handle, material->pipe.pipeline, state->allocator);
//     vkDestroyPipelineLayout(state->device.handle, material->pipe.layout, state->allocator);
//     vkDestroyDescriptorSetLayout(state->device.handle, material->set_layout, state->allocator);
// }

// u32 mat_instance_create(renderer_state* state, mat* material, u64 data_size, void* data) {
//     if (material->inst_count >= MAX_MATERIAL_COUNT) {
//         ETERROR("Max material instances reached.");
//         return INVALID_ID;
//     }
//     u32 inst_index = material->inst_count++;

//     u64 inst_data_offset = material->inst_block_size * inst_index;
//     etcopy_memory((u8*)material->inst_data + inst_data_offset, data, data_size);

//     VkDescriptorBufferInfo inst_data_buff_info = {
//         .buffer = material->inst_data_buffer.handle,
//         .offset = inst_data_offset,
//         .range = data_size,
//     };
    
//     VkWriteDescriptorSet inst_buff_write = {
//         .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
//         .pNext = 0,
//         .descriptorCount = 1,
//         .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//         .dstArrayElement = inst_index,
//         .dstSet = material->pipe.set,
//         .dstBinding = MAT_INSTANCE_UNIFORM_ARRAY,
//         .pBufferInfo = &inst_data_buff_info,
//     };

//     vkUpdateDescriptorSets(
//         state->device.handle,
//         /* WriteCount */ 1,
//         &inst_buff_write,
//         /* CopyCount */ 0,
//         /* Copies */ NULL
//     );

//     return inst_index;
// }