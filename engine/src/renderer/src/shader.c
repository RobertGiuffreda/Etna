#include "shader.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "core/etstring.h"
#include "core/etfile.h"

#include "renderer/src/renderer.h"
#include "renderer/src/utilities/vkinit.h"

#include <spirv_reflect.h>

#define SPIRV_REFLECT_CHECK(expr) { ETASSERT((expr) == SPV_REFLECT_RESULT_SUCCESS); }

static VkDescriptorType spv_reflect_descriptor_to_vulkan_descriptor(SpvReflectDescriptorType descriptor_type);
static VkFormat spv_reflect_format_to_vulkan_format(SpvReflectFormat format);
static VkShaderStageFlagBits spv_reflect_shader_stage_to_vulkan_shader_stage(SpvReflectShaderStageFlagBits stage);

b8 load_shader(renderer_state* state, const char* path, shader* out_shader) {
    if (!file_exists(path)) {
        ETERROR("Unable to find shader file: '%s'.", path);
        return false;
    }

    etfile* shader_file = 0;
    if (!file_open(path, FILE_READ_FLAG | FILE_BINARY_FLAG, &shader_file)) {
        ETERROR("Unable to open the shader file '%s'.", path);
        return false;
    }

    u64 code_size = 0;
    if (!file_size(shader_file, &code_size)) {
        ETERROR("Unable to measure size of compute shader file: '%s'.", path);
        file_close(shader_file);
        return false;
    }

    void* shader_code = etallocate(code_size, MEMORY_TAG_RENDERER);
    if (!file_read_bytes(shader_file, shader_code, &code_size)) {
        etfree(shader_code, code_size, MEMORY_TAG_RENDERER);
        file_close(shader_file);
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
    file_close(shader_file);

    out_shader->entry_point = str_duplicate_allocate(spv_reflect_module.entry_point_name);
    out_shader->stage = spv_reflect_shader_stage_to_vulkan_shader_stage(spv_reflect_module.shader_stage);

    u32 set_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, 0));
    SpvReflectDescriptorSet** descriptor_sets = etallocate(sizeof(SpvReflectDescriptorSet*) * set_count, MEMORY_TAG_SHADER);
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, descriptor_sets));
 
    u32 push_block_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&spv_reflect_module, &push_block_count, 0));
    SpvReflectBlockVariable** push_blocks = etallocate(sizeof(SpvReflectBlockVariable*) * push_block_count, MEMORY_TAG_SHADER);
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&spv_reflect_module, &push_block_count, push_blocks));

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
            curr_binding->descriptor_type = spv_reflect_descriptor_to_vulkan_descriptor(descriptor_sets[i]->bindings[j]->descriptor_type);
            curr_binding->count = descriptor_sets[i]->bindings[j]->count;
        }
    }

    out_shader->push_constant_count = push_block_count;
    out_shader->push_constants = etallocate(sizeof(out_shader->push_constants[0]) * push_block_count, MEMORY_TAG_SHADER);
    // TODO: Record push constant block information

    out_shader->input_count = input_variable_count;
    out_shader->inputs = etallocate(sizeof(out_shader->inputs[0]) * input_variable_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < input_variable_count; ++i) {
        out_shader->inputs[i].format = spv_reflect_format_to_vulkan_format(input_variables[i]->format);
        out_shader->inputs[i].location = input_variables[i]->location;
    }
    
    out_shader->output_count = output_variable_count;
    out_shader->outputs = etallocate(sizeof(out_shader->outputs[0]) * output_variable_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < output_variable_count; ++i) {
        out_shader->outputs[i].format = spv_reflect_format_to_vulkan_format(output_variables[i]->format);
        out_shader->outputs[i].location = output_variables[i]->location;
    }

    // Clean up clean up everybody clean up
    etfree(output_variables, sizeof(SpvReflectInterfaceVariable*) * output_variable_count, MEMORY_TAG_SHADER);
    etfree(input_variables, sizeof(SpvReflectInterfaceVariable*) * input_variable_count, MEMORY_TAG_SHADER);
    etfree(push_blocks, sizeof(SpvReflectBlockVariable*) * push_block_count, MEMORY_TAG_SHADER);
    etfree(descriptor_sets, sizeof(SpvReflectDescriptorSet*) * set_count, MEMORY_TAG_SHADER);
    spvReflectDestroyShaderModule(&spv_reflect_module);
    return true;
}

b8 load_shader2_tests(const char* path, shader2* shader) {
    if (!file_exists(path)) {
        ETERROR("Unable to find shader file: '%s'.", path);
        return false;
    }

    etfile* shader_file = 0;
    if (!file_open(path, FILE_READ_FLAG | FILE_BINARY_FLAG, &shader_file)) {
        ETERROR("Unable to open the shader file '%s'.", path);
        return false;
    }

    u64 code_size = 0;
    if (!file_size(shader_file, &code_size)) {
        ETERROR("Unable to measure size of compute shader file: '%s'.", path);
        file_close(shader_file);
        return false;
    }

    void* shader_code = etallocate(code_size, MEMORY_TAG_RENDERER);
    if (!file_read_bytes(shader_file, shader_code, &code_size)) {
        etfree(shader_code, code_size, MEMORY_TAG_RENDERER);
        file_close(shader_file);
        ETERROR("Error reading bytes from shader code.");
        return false;
    }

    // Shader byte code loaded from file
    SpvReflectShaderModule spv_reflect_module = {0};
    SPIRV_REFLECT_CHECK(spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NONE, code_size, shader_code, &spv_reflect_module));

    // Clean up memoory allocated for shader code and files
    etfree(shader_code, code_size, MEMORY_TAG_RENDERER);
    file_close(shader_file);

    shader->entry_point = str_duplicate_allocate(spv_reflect_module.entry_point_name);
    shader->stage = spv_reflect_shader_stage_to_vulkan_shader_stage(spv_reflect_module.shader_stage);

    u32 set_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, 0));
    SpvReflectDescriptorSet** sets = etallocate(sizeof(SpvReflectDescriptorSet*) * set_count, MEMORY_TAG_SHADER);
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, sets));
 
    u32 push_block_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&spv_reflect_module, &push_block_count, 0));
    SpvReflectBlockVariable** push_blocks = etallocate(sizeof(SpvReflectBlockVariable*) * push_block_count, MEMORY_TAG_SHADER);
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&spv_reflect_module, &push_block_count, push_blocks));

    // u32 set = 0;
    // while (set < set_count) {
    //     ETINFO("Set: %lu", set);
    //     u32 binding = 0;
    //     while (binding < sets[set]->binding_count) {
    //         SpvReflectDescriptorBinding* bind = sets[set]->bindings[binding];
    //         ETINFO("Binding: %lu", binding);
    //         ETINFO("  Binding Name: %s", bind->name);
    //         ETINFO("  Block Name: %s", bind->block.name);
    //         ETINFO("  Descriptor Type: %#010x", bind->descriptor_type);
    //         ETINFO("  Descriptor Type Flags: %#010x", bind->type_description->type_flags);
    //         ETINFO("  Descriptor Type name: %s", bind->type_description->type_name);
    //         ETINFO("    Image dim: ");
    //         for (u32 i = 0; i < bind->block.member_count; ++i) {
    //             SpvReflectBlockVariable member = bind->block.members[i];
    //             ETINFO("  Block member %lu", i);
    //             ETINFO("    Member Name: %s", member.name);
    //             ETINFO("    Member Type flags: %#010x", member.type_description->type_flags);
    //             ETINFO("    Member Type name: %s", member.type_description->type_name);
                
    //             struct Traits* traits = &member.type_description->traits;
    //             ETINFO("    Scalar width: %lu", traits->numeric.scalar.width);
    //             ETINFO("    Scalar signedness: %lu", traits->numeric.scalar.signedness);
    //         }
    //         binding++;
    //     }
    //     set++;
    // }

    shader->set_count = set_count;
    shader->sets = etallocate(sizeof(set_layout) * set_count, MEMORY_TAG_SHADER);
    for (u32 i = 0; i < set_count; ++i) {
        set_layout* set = &shader->sets[i];
        SpvReflectDescriptorSet* spv_set = sets[i];

        set->index = spv_set->set;
        set->binding_count = spv_set->binding_count;
        set->bindings = etallocate(sizeof(binding_layout) * spv_set->binding_count, MEMORY_TAG_SHADER);

        for (u32 j = 0; j < set->binding_count; ++j) {
            binding_layout* binding = &set->bindings[j];
            SpvReflectDescriptorBinding* spv_binding = spv_set->bindings[j];
            
            binding->name = str_duplicate_allocate(spv_binding->name);
            binding->index = spv_binding->binding;
            
            // If this descriptor is a block
            if (spv_binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER ||
                spv_binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC ||
                spv_binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER ||
                spv_binding->descriptor_type == SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC)
            {
                binding->has_block = true;
                binding->block.member_count = spv_binding->block.member_count;
                binding->block.members = etallocate(
                    sizeof(block_variable) * spv_binding->block.member_count,
                    MEMORY_TAG_SHADER);
                block_variable* c_block = &binding->block.members[0];
            }
        }
    }

    etfree(push_blocks, sizeof(SpvReflectBlockVariable*) * push_block_count, MEMORY_TAG_SHADER);
    etfree(sets, sizeof(SpvReflectDescriptorSet*) * set_count, MEMORY_TAG_SHADER);
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

VkDescriptorType spv_reflect_descriptor_to_vulkan_descriptor(SpvReflectDescriptorType descriptor_type) {
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
        ETERROR("Unknown SpvReflectDescriptorType value passed to spv_reflect_descriptor_to_vulkan_descriptor.");
        return 0;
        break;
    }
}

VkFormat spv_reflect_format_to_vulkan_format(SpvReflectFormat format) {
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
        ETERROR("Unknown SpvReflectFormat value passed to spv_reflect_format_to_vulkan_format.");
        return VK_FORMAT_UNDEFINED;
        break;
    }
}

VkShaderStageFlagBits spv_reflect_shader_stage_to_vulkan_shader_stage(SpvReflectShaderStageFlagBits stage) {
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
        ETERROR("Unknown SpvReflectShaderStageFlagBits value passed to spv_reflect_shader_stage_to_vulkan_shader_stage.");
        return 0;
        break;
    }
}