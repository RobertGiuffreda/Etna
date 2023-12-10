#pragma once

#include "renderer_types.h"

// TODO: Better isolate this library (spirv reflect) in the system to make
// changing it out to something else like SPIRV-Cross easier if it becomes
// depricated.
#include <spirv_reflect.h>

#define SPIRV_REFLECT_CHECK(expr) { ETASSERT((expr) == SPV_REFLECT_RESULT_SUCCESS); }

/** Information needed for each binding:
 * Requisite information for:
 * VkPipelineVertexInputStateCreateInfo
 * TODO: VkPipelineTessellationStateCreateInfo
 */
VkPipelineVertexInputStateCreateInfo;
typedef struct shader {
    VkShaderModule module;
    struct {
        VkShaderStageFlagBits stage;
        char* entry_point;
        u32 set_count;
        struct {
            u32 set;
            u32 binding_count;
            struct {
                u32 set;
                u32 binding;
                VkDescriptorType descriptor_type;
                u32 count;
                struct {
                    VkImageType dim;
                    VkFormat format;
                }image;
            }*bindings;
        } *sets;

        // TODO: Get information about push constants
        u32 push_constant_count;
        struct {
            u32 temp;
        }*push_constants;

        // TODO: Add type information maybe
        u32 input_count;
        struct {
            u32 location;
            VkFormat format;
        }*inputs;

        // TODO: Add type information maybe
        u32 output_count;
        struct {
            u32 location;
            VkFormat format;
        }*outputs;
    }reflection;
} shader;

b8 load_shader(renderer_state* state, const char* path, shader* out_shader);

void unload_shader(renderer_state* state, shader* shader);