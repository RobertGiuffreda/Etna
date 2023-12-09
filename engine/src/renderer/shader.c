#include "shader.h"

#include "core/etmemory.h"
#include "core/logger.h"
#include "platform/filesystem.h"

#include "renderer/utilities/vkinit.h"

typedef struct spv_reflect_data {
    SpvReflectShaderModule module;
    SpvReflectDescriptorSet* sets;
} spv_reflect_data;

// TODO: Push constant reflection data
// TODO: Uniform buffer reflection data
// TODO: Storage buffer data
b8 load_shader(const char* path, shader* out_shader) {
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
    // Shader byte code loaded from file
    SpvReflectShaderModule spv_reflect_module = {0};
    SPIRV_REFLECT_CHECK(spvReflectCreateShaderModule2(SPV_REFLECT_MODULE_FLAG_NONE, code_size, shader_code, &spv_reflect_module));

    u32 set_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, 0));
    SpvReflectDescriptorSet** descriptor_sets = etallocate(sizeof(SpvReflectDescriptorSet*), MEMORY_TAG_RENDERER);
    SPIRV_REFLECT_CHECK(spvReflectEnumerateDescriptorSets(&spv_reflect_module, &set_count, descriptor_sets));

    u32 push_constant_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstants(&spv_reflect_module, &push_constant_count, 0));
    SpvReflectBlockVariable** pushs = etallocate(sizeof(SpvReflectBlockVariable*) * push_constant_count, MEMORY_TAG_RENDERER);
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstants(&spv_reflect_module, &push_constant_count, pushs));

    u32 push_constant_block_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&spv_reflect_module, &push_constant_block_count, 0));
    SpvReflectBlockVariable** push_blocks = etallocate(sizeof(SpvReflectBlockVariable*) * push_constant_block_count, MEMORY_TAG_RENDERER);
    SPIRV_REFLECT_CHECK(spvReflectEnumeratePushConstantBlocks(&spv_reflect_module, &push_constant_block_count, push_blocks));

    u32 interface_variables_count = 0;
    SPIRV_REFLECT_CHECK(spvReflectEnumerateInterfaceVariables(&spv_reflect_module, &interface_variables_count, 0));
    SpvReflectInterfaceVariable** interface_variables = etallocate(sizeof(SpvReflectInterfaceVariable*) * interface_variables_count, MEMORY_TAG_RENDERER);
    SPIRV_REFLECT_CHECK(spvReflectEnumerateInterfaceVariables(&spv_reflect_module, &interface_variables_count, interface_variables));

    for (u32 i = 0; i < set_count; ++i) {
        for (u32 j = 0; j < descriptor_sets[i]->binding_count; ++j) {
            ETINFO("Set %lu: Binding %lu: %s", descriptor_sets[i]->set, descriptor_sets[i]->bindings[j]->binding, descriptor_sets[i]->bindings[j]->name);
        }
    }

    for (u32 i = 0; i < push_constant_count; ++i) {
        ETINFO("Push %d: %s.", i, pushs[i]->name);
    }

    for (u32 i = 0; i < push_constant_block_count; ++i) {
        ETINFO("Push block %d: %s.", i, push_blocks[i]->name);
    }

    for (u32 i = 0; i < interface_variables_count; ++i) {
        ETINFO("Interface variable %d: %s: %s.", i, interface_variables[i]->name, interface_variables[i]->type_description->type_name);
    }

    // Store descriptor set layout creation information

    // Clean up clean up everybody clean up 
    etfree(interface_variables, sizeof(SpvReflectInterfaceVariable*) * interface_variables_count, MEMORY_TAG_RENDERER);
    etfree(push_blocks, sizeof(SpvReflectBlockVariable*) * push_constant_block_count, MEMORY_TAG_RENDERER);
    etfree(pushs, sizeof(SpvReflectBlockVariable*) * push_constant_count, MEMORY_TAG_RENDERER);
    etfree(descriptor_sets, sizeof(SpvReflectDescriptorSet*) * set_count, MEMORY_TAG_RENDERER);
    filesystem_close(shader_file);
    spvReflectDestroyShaderModule(&spv_reflect_module);
    etfree(shader_code, code_size, MEMORY_TAG_RENDERER);
    return true;
}