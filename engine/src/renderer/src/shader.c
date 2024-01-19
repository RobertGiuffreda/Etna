#include "shader.h"


#include "core/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"
#include "platform/filesystem.h"

#include "renderer/src/renderer.h"
#include "renderer/src/utilities/vkinit.h"

#include <spirv_reflect.h>

#define SPIRV_REFLECT_CHECK(expr) { ETASSERT((expr) == SPV_REFLECT_RESULT_SUCCESS); }

static VkDescriptorType reflect_descriptor_to_vulkan_descriptor(SpvReflectDescriptorType descriptor_type);
static VkFormat reflect_format_to_vulkan_format(SpvReflectFormat format);
static VkShaderStageFlagBits reflect_shader_stage_to_vulkan_shader_stage(SpvReflectShaderStageFlagBits stage);

// TODO: Push constant reflection data
// TODO: Uniform buffer reflection data
// TODO: Storage buffer data
b8 load_shader(renderer_state* state, const char* path, shader* out_shader) {
    // Load passed in shader file from config
    if (!filesystem_exists(path)) {
        ETERROR("Unable to find shader file: '%s'.", path);
        return false;
    }

    etfile* shader_file = 0;
    if (!filesystem_open(path, FILE_READ_FLAG | FILE_BINARY_FLAG, &shader_file)) {
        ETERROR("Unable to open the shader file '%s'.", path);
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

    out_shader->entry_point = str_duplicate_allocate(spv_reflect_module.entry_point_name);
    out_shader->stage = reflect_shader_stage_to_vulkan_shader_stage(spv_reflect_module.shader_stage);

    u32 set_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, 0));
    SpvReflectDescriptorSet** descriptor_sets = etallocate(sizeof(SpvReflectDescriptorSet*) * set_count, MEMORY_TAG_SHADER);
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


    // Enumerate set count but only enumerate the bindings
    out_shader->set_count = set_count;
    out_shader->_sets = etallocate(sizeof(set) * set_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < set_count; ++i) {
        // Pointer for easier to read code
        set* curr_set = &out_shader->_sets[i];

        curr_set->set = descriptor_sets[i]->set;
        curr_set->binding_count = descriptor_sets[i]->binding_count;
        curr_set->_bindings = etallocate(sizeof(binding) * curr_set->binding_count, MEMORY_TAG_SHADER);
        for (u32 j = 0; j < curr_set->binding_count; ++j) {
            // Pointer for easier to read code
            binding* curr_binding = &curr_set->_bindings[j];
            curr_binding->binding = descriptor_sets[i]->bindings[j]->binding;
            curr_binding->descriptor_type = reflect_descriptor_to_vulkan_descriptor(descriptor_sets[i]->bindings[j]->descriptor_type);
            curr_binding->count = descriptor_sets[i]->bindings[j]->count;
        }
    }

    // TODO: Gather push count information
    out_shader->push_constant_count = push_constant_count;
    out_shader->push_constants = etallocate(sizeof(out_shader->push_constants[0]) * push_constant_count, MEMORY_TAG_SHADER);

    out_shader->input_count = input_variable_count;
    out_shader->inputs = etallocate(sizeof(out_shader->inputs[0]) * input_variable_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < input_variable_count; ++i) {
        out_shader->inputs[i].format = reflect_format_to_vulkan_format(input_variables[i]->format);
        out_shader->inputs[i].location = input_variables[i]->location;
    }
    
    out_shader->output_count = output_variable_count;
    out_shader->outputs = etallocate(sizeof(out_shader->outputs[0]) * output_variable_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < output_variable_count; ++i) {
        out_shader->outputs[i].format = reflect_format_to_vulkan_format(output_variables[i]->format);
        out_shader->outputs[i].location = output_variables[i]->location;
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
    etfree(shader->outputs, sizeof(shader->outputs[0]) * shader->output_count, MEMORY_TAG_SHADER);
    etfree(shader->inputs, sizeof(shader->inputs[0]) * shader->input_count, MEMORY_TAG_SHADER);
    etfree(shader->push_constants, sizeof(shader->push_constants[0]) * shader->push_constant_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < shader->set_count; ++i) {
        etfree(shader->_sets[i]._bindings, sizeof(binding) * shader->_sets[i].binding_count, MEMORY_TAG_SHADER);
    }
    etfree(shader->_sets, sizeof(set) * shader->set_count, MEMORY_TAG_SHADER);
    str_duplicate_free(shader->entry_point);
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