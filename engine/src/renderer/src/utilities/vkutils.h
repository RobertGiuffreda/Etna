#pragma once

#include "renderer/src/vk_types.h"

i32 find_memory_index(const VkPhysicalDeviceMemoryProperties* memory_properties,
    u32 memory_type_bits_requirement, VkMemoryPropertyFlags required_properties);

mesh_buffers upload_mesh_immediate(
    renderer_state* state,
    u32 index_count, u32* indices, 
    u32 vertex_count, vertex* vertices);