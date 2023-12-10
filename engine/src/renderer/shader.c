#include "shader.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"
#include "platform/filesystem.h"

#include "renderer/utilities/vkinit.h"

static VkDescriptorType reflect_descriptor_to_vulkan_descriptor(SpvReflectDescriptorType descriptor_type);
static VkFormat reflect_format_to_vulkan_format(SpvReflectFormat format);
static VkShaderStageFlagBits reflect_shader_stage_to_vulkan_shader_stage(SpvReflectShaderStageFlagBits stage);

// TODO: Push constant reflection data
// TODO: Uniform buffer reflection data
// TODO: Storage buffer data
b8 load_shader(renderer_state* state, const char* path, shader* out_shader) {
    // Load passed in shader file from config
    if (!filesystem_exists(path)) {
        ETERROR("Unable to find compute shader file: '%s'.", path);
        return false;
    }

    etfile* shader_file = 0;
    if (!filesystem_open(path, FILE_READ_FLAG | FILE_BINARY_FLAG, &shader_file)) {
        ETERROR("Unable to open the compute shader file '%s'.", path);
        return false;
    }

    u64 code_size = 0;
    if (!filesystem_size(shader_file, &code_size)) {
        ETERROR("Unable to measure size of compute shader file: '%s'.", path);
        filesystem_close(shader_file);
        return false;
    }

    void* shader_code = etallocate(code_size, MEMORY_TAG_RENDERER);
    if (!filesystem_read_all_bytes(shader_file, shader_code, &code_size)) {
        etfree(shader_code, code_size, MEMORY_TAG_RENDERER);
        filesystem_close(shader_file);
        ETERROR("Error reading bytes from shader code.");
        return false;
    }
    VkShaderModuleCreateInfo shader_info = init_shader_module_create_info();
    shader_info.codeSize = code_size;
    shader_info.pCode = (u32*)shader_code;

    VK_CHECK(vkCreateShaderModule(state->device.handle, &shader_info, state->allocator, &out_shader->module));

    // Shader byte code loaded from file
    SpvReflectShaderModule spv_reflect_module = {0};
    SPIRV_REFLECT_CHECK(spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NONE, code_size, shader_code, &spv_reflect_module));

    // Clean up memoory allocated for shader code and files
    etfree(shader_code, code_size, MEMORY_TAG_RENDERER);
    filesystem_close(shader_file);

    out_shader->reflection.entry_point = string_duplicate_allocate(spv_reflect_module.entry_point_name);
    out_shader->reflection.stage = reflect_shader_stage_to_vulkan_shader_stage(spv_reflect_module.shader_stage);

    u32 set_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, 0));
    SpvReflectDescriptorSet** descriptor_sets = etallocate(sizeof(SpvReflectDescriptorSet*), MEMORY_TAG_SHADER);
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, descriptor_sets));

    u32 push_constant_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstants(&spv_reflect_module, &push_constant_count, 0));
    SpvReflectBlockVariable** pushs = etallocate(sizeof(SpvReflectBlockVariable*) * push_constant_count, MEMORY_TAG_SHADER);
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstants(&spv_reflect_module, &push_constant_count, pushs));

    u32 input_variable_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateInputVariables(&spv_reflect_module, &input_variable_count, 0));
    SpvReflectInterfaceVariable** input_variables = etallocate(sizeof(SpvReflectInterfaceVariable*) * input_variable_count, MEMORY_TAG_SHADER);
    SPIRV_REFLECT_CHECK(spvReflectEnumerateInputVariables(&spv_reflect_module, &input_variable_count, input_variables));

    u32 output_variable_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateOutputVariables(&spv_reflect_module, &output_variable_count, 0));
    SpvReflectInterfaceVariable** output_variables = etallocate(sizeof(SpvReflectInterfaceVariable*) * output_variable_count, MEMORY_TAG_SHADER);
    SPIRV_REFLECT_CHECK(spvReflectEnumerateOutputVariables(&spv_reflect_module, &output_variable_count, output_variables));

    out_shader->reflection.set_count = set_count;
    out_shader->reflection.sets = etallocate(sizeof(out_shader->reflection.sets[0]) * set_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < set_count; ++i) {
        out_shader->reflection.sets[i].set = descriptor_sets[i]->set;
        out_shader->reflection.sets[i].binding_count = descriptor_sets[i]->binding_count;
        out_shader->reflection.sets[i].bindings = etallocate(sizeof(out_shader->reflection.sets[i].bindings[0]) * descriptor_sets[i]->binding_count, MEMORY_TAG_SHADER);
        for (u32 j = 0; j < descriptor_sets[i]->binding_count; ++j) {
            out_shader->reflection.sets[i].bindings[j].set = descriptor_sets[i]->bindings[j]->set;
            out_shader->reflection.sets[i].bindings[j].binding = descriptor_sets[i]->bindings[j]->binding;
            out_shader->reflection.sets[i].bindings[j].descriptor_type = reflect_descriptor_to_vulkan_descriptor(descriptor_sets[i]->bindings[j]->descriptor_type);
            out_shader->reflection.sets[i].bindings[j].count = descriptor_sets[i]->bindings[j]->count;
        }
    }

    // TODO: Gather push count information
    out_shader->reflection.push_constant_count = push_constant_count;
    out_shader->reflection.push_constants = etallocate(sizeof(out_shader->reflection.push_constants[0]) * push_constant_count, MEMORY_TAG_SHADER);

    out_shader->reflection.input_count = input_variable_count;
    out_shader->reflection.inputs = etallocate(sizeof(out_shader->reflection.inputs[0]) * input_variable_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < input_variable_count; ++i) {
        out_shader->reflection.inputs[i].format = reflect_format_to_vulkan_format(input_variables[i]->format);
        out_shader->reflection.inputs[i].location = input_variables[i]->location;
    }
    
    out_shader->reflection.output_count = output_variable_count;
    out_shader->reflection.outputs = etallocate(sizeof(out_shader->reflection.outputs[0]) * output_variable_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < output_variable_count; ++i) {
        out_shader->reflection.outputs[i].format = reflect_format_to_vulkan_format(output_variables[i]->format);
        out_shader->reflection.outputs[i].location = output_variables[i]->location;
    }

    // Store relevant descriptor set layout creation information in out_shader
    // Iterate through and do not count builtin interface variables information

    // See difference between push constant blocks and push constants
    // u32 push_constant_block_count = 0;
    // SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&spv_reflect_module, &push_constant_block_count, 0));
    // SpvReflectBlockVariable** push_blocks = etallocate(sizeof(SpvReflectBlockVariable*) * push_constant_block_count, MEMORY_TAG_SHADER);
    // SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&spv_reflect_module, &push_constant_block_count, push_blocks));
    // etfree(push_blocks, sizeof(SpvReflectBlockVariable*) * push_constant_block_count, MEMORY_TAG_SHADER);

    // Clean up clean up everybody clean up
    etfree(output_variables, sizeof(SpvReflectInterfaceVariable*) * output_variable_count, MEMORY_TAG_SHADER);
    etfree(input_variables, sizeof(SpvReflectInterfaceVariable*) * input_variable_count, MEMORY_TAG_SHADER);
    etfree(pushs, sizeof(SpvReflectBlockVariable*) * push_constant_count, MEMORY_TAG_SHADER);
    etfree(descriptor_sets, sizeof(SpvReflectDescriptorSet*) * set_count, MEMORY_TAG_SHADER);
    spvReflectDestroyShaderModule(&spv_reflect_module);
    return true;
}

void unload_shader(renderer_state* state, shader* shader) {
    etfree(shader->reflection.outputs, sizeof(shader->reflection.outputs[0]) * shader->reflection.output_count, MEMORY_TAG_SHADER);
    etfree(shader->reflection.inputs, sizeof(shader->reflection.inputs[0]) * shader->reflection.input_count, MEMORY_TAG_SHADER);
    etfree(shader->reflection.push_constants, sizeof(shader->reflection.push_constants[0]) * shader->reflection.push_constant_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < shader->reflection.set_count; ++i) {
        etfree(shader->reflection.sets[i].bindings, sizeof(shader->reflection.sets[0].bindings[0]) * shader->reflection.sets[i].binding_count, MEMORY_TAG_SHADER);
    }
    etfree(shader->reflection.sets, sizeof(shader->reflection.sets[0]) * shader->reflection.set_count, MEMORY_TAG_SHADER);
    string_duplicate_free(shader->reflection.entry_point);
    vkDestroyShaderModule(state->device.handle, shader->module, state->allocator);
}

VkDescriptorType reflect_descriptor_to_vulkan_descriptor(SpvReflectDescriptorType descriptor_type) {
    switch (descriptor_type)
    {
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_SAMPLER;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
        return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
        return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
        return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
        return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
        return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
        return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        break;
    case SPV_REFLECT_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR:
        return VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR;
        break;
    default:
        ETERROR("Unknown SpvReflectDescriptorType value passed to reflect_descriptor_to_vulkan_descriptor.");
        return 0;
        break;
    }
}

VkFormat reflect_format_to_vulkan_format(SpvReflectFormat format) {
    switch (format)
    {
    case SPV_REFLECT_FORMAT_UNDEFINED:
    return VK_FORMAT_UNDEFINED;
        break;
    case SPV_REFLECT_FORMAT_R16_UINT:
    return VK_FORMAT_R16_UINT;
        break;
    case SPV_REFLECT_FORMAT_R16_SINT:
    return VK_FORMAT_R16_SINT;
        break;
    case SPV_REFLECT_FORMAT_R16_SFLOAT:
    return VK_FORMAT_R16_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R16G16_UINT:
    return VK_FORMAT_R16G16_UINT;
        break;
    case SPV_REFLECT_FORMAT_R16G16_SINT:
    return VK_FORMAT_R16G16_SINT;
        break;
    case SPV_REFLECT_FORMAT_R16G16_SFLOAT:
    return VK_FORMAT_R16G16_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R16G16B16_UINT:
    return VK_FORMAT_R16G16B16_UINT;
        break;
    case SPV_REFLECT_FORMAT_R16G16B16_SINT:
    return VK_FORMAT_R16G16B16_SINT;
        break;
    case SPV_REFLECT_FORMAT_R16G16B16_SFLOAT:
    return VK_FORMAT_R16G16B16_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R16G16B16A16_UINT:
    return VK_FORMAT_R16G16B16A16_UINT;
        break;
    case SPV_REFLECT_FORMAT_R16G16B16A16_SINT:
    return VK_FORMAT_R16G16B16A16_SINT;
        break;
    case SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R32_UINT:
    return VK_FORMAT_R32_UINT;
        break;
    case SPV_REFLECT_FORMAT_R32_SINT:
    return VK_FORMAT_R32_SINT;
        break;
    case SPV_REFLECT_FORMAT_R32_SFLOAT:
    return VK_FORMAT_R32_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R32G32_UINT:
    return VK_FORMAT_R32G32_UINT;
        break;
    case SPV_REFLECT_FORMAT_R32G32_SINT:
    return VK_FORMAT_R32G32_SINT;
        break;
    case SPV_REFLECT_FORMAT_R32G32_SFLOAT:
    return VK_FORMAT_R32G32_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R32G32B32_UINT:
    return VK_FORMAT_R32G32B32_UINT;
        break;
    case SPV_REFLECT_FORMAT_R32G32B32_SINT:
    return VK_FORMAT_R32G32B32_SINT;
        break;
    case SPV_REFLECT_FORMAT_R32G32B32_SFLOAT:
    return VK_FORMAT_R32G32B32_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R32G32B32A32_UINT:
    return VK_FORMAT_R32G32B32A32_UINT;
        break;
    case SPV_REFLECT_FORMAT_R32G32B32A32_SINT:
    return VK_FORMAT_R32G32B32A32_SINT;
        break;
    case SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R64_UINT:
    return VK_FORMAT_R64_UINT;
        break;
    case SPV_REFLECT_FORMAT_R64_SINT:
    return VK_FORMAT_R64_SINT;
        break;
    case SPV_REFLECT_FORMAT_R64_SFLOAT:
    return VK_FORMAT_R64_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R64G64_UINT:
    return VK_FORMAT_R64G64_UINT;
        break;
    case SPV_REFLECT_FORMAT_R64G64_SINT:
    return VK_FORMAT_R64G64_SINT;
        break;
    case SPV_REFLECT_FORMAT_R64G64_SFLOAT:
    return VK_FORMAT_R64G64_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R64G64B64_UINT:
    return VK_FORMAT_R64G64B64_UINT;
        break;
    case SPV_REFLECT_FORMAT_R64G64B64_SINT:
    return VK_FORMAT_R64G64B64_SINT;
        break;
    case SPV_REFLECT_FORMAT_R64G64B64_SFLOAT:
    return VK_FORMAT_R64G64B64_SFLOAT;
        break;
    case SPV_REFLECT_FORMAT_R64G64B64A64_UINT:
    return VK_FORMAT_R64G64B64A64_UINT;
        break;
    case SPV_REFLECT_FORMAT_R64G64B64A64_SINT:
    return VK_FORMAT_R64G64B64A64_SINT;
        break;
    case SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT:
    return VK_FORMAT_R64G64B64A64_SFLOAT;
        break;
    default:
        ETERROR("Unknown SpvReflectFormat value passed to reflect_format_to_vulkan_format.");
        return VK_FORMAT_UNDEFINED;
        break;
    }
}

VkShaderStageFlagBits reflect_shader_stage_to_vulkan_shader_stage(SpvReflectShaderStageFlagBits stage) {
    switch (stage)
    {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
        return VK_SHADER_STAGE_VERTEX_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_CONTROL_BIT:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_TESSELLATION_EVALUATION_BIT:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_GEOMETRY_BIT:
        return VK_SHADER_STAGE_GEOMETRY_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
        return VK_SHADER_STAGE_FRAGMENT_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
        return VK_SHADER_STAGE_COMPUTE_BIT;
        break;
    case SPV_REFLECT_SHADER_STAGE_TASK_BIT_EXT:
        return VK_SHADER_STAGE_TASK_BIT_EXT;
        break;
    case SPV_REFLECT_SHADER_STAGE_MESH_BIT_EXT:
        return VK_SHADER_STAGE_MESH_BIT_EXT;
        break;
    case SPV_REFLECT_SHADER_STAGE_RAYGEN_BIT_KHR:
        return VK_SHADER_STAGE_RAYGEN_BIT_KHR;
        break;
    case SPV_REFLECT_SHADER_STAGE_ANY_HIT_BIT_KHR:
        return VK_SHADER_STAGE_ANY_HIT_BIT_KHR;
        break;
    case SPV_REFLECT_SHADER_STAGE_CLOSEST_HIT_BIT_KHR:
        return VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR;
        break;
    case SPV_REFLECT_SHADER_STAGE_MISS_BIT_KHR:
        return VK_SHADER_STAGE_MISS_BIT_KHR;
        break;
    case SPV_REFLECT_SHADER_STAGE_INTERSECTION_BIT_KHR:
        return VK_SHADER_STAGE_INTERSECTION_BIT_KHR;
        break;
    case SPV_REFLECT_SHADER_STAGE_CALLABLE_BIT_KHR:
        return VK_SHADER_STAGE_CALLABLE_BIT_KHR;
        break;
    
    default:
        ETERROR("Unknown SpvReflectShaderStageFlagBits value passed to reflect_shader_stage_to_vulkan_shader_stage.");
        return 0;
        break;
    }
}