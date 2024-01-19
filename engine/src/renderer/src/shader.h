#pragma once

#include "renderer/src/vk_types.h"

enum shader_descriptor_type {
    DESCRIPTOR_TYPE_SAMPLER,
    DESCRIPTOR_TYPE_SAMPLED_IMAGE,
    DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
    DESCRIPTOR_TYPE_INPUT_ATTACHMENT,
    DESCRIPTOR_TYPE_STORAGE_IMAGE,
    DESCRIPTOR_TYPE_UNIFORM_BUFFER,
    DESCRIPTOR_TYPE_STORAGE_BUFFER,
};

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
struct shader_struct_element_entry {
    // The name of the entry.
    char* element_name;
    // The type of the entry.
    enum shader_type element_type;
    // The count, if its an array.
    u32 element_count;
    // The struct layout, if its a struct.
    struct shader_struct_layout* struct_layout;
};

struct shader_struct_layout {
    struct shader_struct_element_entry* entries;
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

b8 load_shader(renderer_state* state, const char* path, shader* out_shader);

void unload_shader(renderer_state* state, shader* shader);