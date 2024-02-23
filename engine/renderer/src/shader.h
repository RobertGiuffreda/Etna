#pragma once

#include "renderer/src/vk_types.h"

// NOTE: Taken from SPIRV-Reflect
typedef enum descriptor_type {
    DESCRIPTOR_TYPE_SAMPLER                    =  0,        // = VK_DESCRIPTOR_TYPE_SAMPLER
    DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER     =  1,        // = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
    DESCRIPTOR_TYPE_SAMPLED_IMAGE              =  2,        // = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE
    DESCRIPTOR_TYPE_STORAGE_IMAGE              =  3,        // = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
    DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER       =  4,        // = VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER
    DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER       =  5,        // = VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER
    DESCRIPTOR_TYPE_UNIFORM_BUFFER             =  6,        // = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
    DESCRIPTOR_TYPE_STORAGE_BUFFER             =  7,        // = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
    DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC     =  8,        // = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
    DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC     =  9,        // = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT           = 10,        // = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT
    DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR = 1000150000 // = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR
} descriptor_type;

// NOTE: Taken from SPIRV-Reflect
typedef enum type_flag_bits {
    TYPE_FLAG_UNDEFINED                       = 0x00000000,
    TYPE_FLAG_VOID                            = 0x00000001,
    TYPE_FLAG_BOOL                            = 0x00000002,
    TYPE_FLAG_INT                             = 0x00000004,
    TYPE_FLAG_FLOAT                           = 0x00000008,
    TYPE_FLAG_VECTOR                          = 0x00000100,
    TYPE_FLAG_MATRIX                          = 0x00000200,
    TYPE_FLAG_EXTERNAL_IMAGE                  = 0x00010000,
    TYPE_FLAG_EXTERNAL_SAMPLER                = 0x00020000,
    TYPE_FLAG_EXTERNAL_SAMPLED_IMAGE          = 0x00040000,
    TYPE_FLAG_EXTERNAL_BLOCK                  = 0x00080000,
    TYPE_FLAG_EXTERNAL_ACCELERATION_STRUCTURE = 0x00100000,
    TYPE_FLAG_EXTERNAL_MASK                   = 0x00FF0000,
    TYPE_FLAG_STRUCT                          = 0x10000000,
    TYPE_FLAG_ARRAY                           = 0x20000000,
    TYPE_FLAG_REF                             = 0x40000000
} type_flag_bits;
typedef u32 type_flags;

typedef enum image_dim {
    IMAGE_DIM_1D = 0,
    IMAGE_DIM_2D = 1,
    IMAGE_DIM_3D = 2,
    IMAGE_DIM_CUBE = 3
} image_dim;

// TODO: More comprehensive
// typedef enum image_format {
//     IMAGE_FORMAT_UNKNOWN = 0,
//     IMAGE_FORMAT_RGBA32F = 1,
//     IMAGE_FORMAT_RGBA16F = 2,
// } image_format;

typedef struct block_variable block_variable;
struct block_variable {
    char* name;
    u32 offset;
    u32 absolute_offset;
    u32 size;
    u32 padded_size;

    // Numeric traits
    struct {
        u32 signedness;
    } scalar;

    struct {
        u32 component_count;
    } vector;

    struct {
        u32 column_count;
        u32 row_count;
        u32 stride;
    } matrix;

    struct {
        u32 dim_count;
        u32 dim_lengths[32];
        u32 stride;
    } array;

    type_flags flags;    

    // If variable type has struct
    u32 member_count;
    block_variable* members;
};

typedef struct binding_layout {
    u32 index;
    char* name;
    descriptor_type descriptor_type;
    struct {
        image_dim dim;
        u32 depth;
        u32 array;
        u32 multisampling;
        u32 sampled;
        // image_format format;
    } image;
    block_variable block;
    u32 count;
    u32 accessed;
} binding_layout;

// TODO: Add image count, buffer count
typedef struct set_layout {
    u32 index;
    u32 binding_count;
    binding_layout* bindings;
} set_layout;

typedef struct shader {
    VkShaderModule module;
    VkShaderStageFlagBits stage;
    char* entry_point;

    // DescriptorSet information
    u32 set_count;
    set_layout* sets;

    // Push constant information
    u32 push_block_count;
    block_variable* push_blocks;
} shader;

// NOTE: Reflection only tested with Vulkan GLSL
b8 load_shader(renderer_state* state, const char* path, shader* shader);
void unload_shader(renderer_state* state, shader* shader);

void print_shader_info(shader* shader);
void print_push_constants(shader* shader);
void print_block_variables(block_variable* block);
void print_set_layout(set_layout* set);
void print_binding_layout(binding_layout* binding);

void print_sets_and_bindings(shader* shader);