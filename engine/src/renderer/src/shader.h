#pragma once

#include "renderer/src/vk_types.h"

// enum shader_descriptor_type {
//     DESCRIPTOR_TYPE_SAMPLER,
//     DESCRIPTOR_TYPE_SAMPLED_IMAGE,
//     DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
//     DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
//     DESCRIPTOR_TYPE_STORAGE_IMAGE,
//     DESCRIPTOR_TYPE_UNIFORM_BUFFER,
//     DESCRIPTOR_TYPE_STORAGE_BUFFER,
// };

enum shader_type {
    SHADER_TYPE_FLOAT,
    SHADER_TYPE_VEC2,
    SHADER_TYPE_VEC3,
    SHADER_TYPE_VEC4,
    SHADER_TYPE_MAT2,
    SHADER_TYPE_MAT3,
    SHADER_TYPE_MAT4,
    SHADER_TYPE_INT,
    SHADER_TYPE_IVEC2,
    SHADER_TYPE_IVEC3,
    SHADER_TYPE_IVEC4,
    SHADER_TYPE_STRUCT,
};

// TODO: Support for more formats
struct shader_struct_element {
    // The name of the entry.
    char* name;
    // The type of the entry.
    enum shader_type element_type;
    // The count, if its an array.
    u32 element_count;
    // The struct layout, if its a struct.
    struct shader_struct_layout* struct_layout;
};

struct shader_struct_layout {
    struct shader_struct_element* entries;
    u32 entry_count;
};

struct shader_descriptor_set_entry {
    char* name;
    enum shader_descriptor_type type;
    u32 descriptor_count;
    struct shader_struct_layout* struct_layout;
};

struct shader_descriptor_set_layout {
    struct shader_descriptor_set_entry* descriptor_set_entries;
    u32 descriptor_set_entry_count;
};

typedef struct binding {
    u32 binding;
    VkDescriptorType descriptor_type;
    u32 count;
    struct {
        VkImageType dim;
        VkFormat format;
    } image;
} binding;

typedef struct set {
    u32 set;
    // Dynarray
    u32 binding_count;
    binding* _bindings;
} set;

typedef struct shader {
    VkShaderModule module;
    VkShaderStageFlagBits stage;
    char* entry_point;

    // Descriptor Set Reflection info
    u32 set_count;
    set* _sets;

    // TODO: Get information about push constants
    u32 push_constant_count;
    struct {
        u32 temp;
    }*push_constants;

    u32 input_count;
    struct {
        u32 location;
        VkFormat format;
    }*inputs;

    u32 output_count;
    struct {
        u32 location;
        VkFormat format;
    }*outputs;
} shader;

typedef enum descriptor_type {
    DESCRIPTOR_TYPE_SAMPLER,
    DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
    DESCRIPTOR_TYPE_STORAGE_IMAGE,
    DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    DESCRIPTOR_TYPE_STORAGE_BUFFER,
} descriptor_type;

typedef enum type_flag_bits {
    TYPE_FLAG_UNDEFINED = 0x00000000,
    TYPE_FLAG_VOID = 0x00000001,
    TYPE_FLAG_FLOAT = 0x00000002,
    TYPE_FLAG_INT = 0x00000004,
    TYPE_FLAG_VECTOR = 0x00000008,
    TYPE_FLAG_MATRIX = 0x00000010,
    TYPE_FLAG_STRUCT = 0x00000020,
    TYPE_FLAG_ARRAY = 0x00000040,
    TYPE_FLAG_REF = 0x00000080,
} type_flag_bits;
typedef u32 type_flags;

typedef enum image_dim {
    IMAGE_DIM_1D,
    IMAGE_DIM_2D,
    IMAGE_DIM_3D,
    IMAGE_DIM_CUBE
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
    } numeric;

    // Array traits
    struct {
        u32 dim_count;
        u32 dim_lengths[32];
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
    b8 has_block;
    union {
        struct {
            image_dim dim;
            u32 depth;
            u32 array;
            u32 ms;
            u32 sampled;
            // image_format format;
        } image;
        struct {
            u32 member_count;
            block_variable* members;
        } block; // Or buffer as name
    };
    u32 count;
    u32 accessed;
} binding_layout;


typedef struct set_layout {
    u32 index;
    u32 binding_count;
    binding_layout* bindings;
} set_layout;

typedef struct shader2 {
    // NOTE: Vulkan stuff
    VkShaderModule module;
    VkShaderStageFlagBits stage;
    char* entry_point;
    // NOTE: END

    // NOTE: Reflection info
    u32 set_count;
    set_layout* sets;
    // NOTE: END

} shader2;

b8 load_shader(renderer_state* state, const char* path, shader* out_shader);

b8 load_shader2_tests(const char* path, shader2* shader);

void unload_shader(renderer_state* state, shader* shader);