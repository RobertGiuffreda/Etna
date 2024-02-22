#pragma once
#include "resource_types.h"

#include "renderer/src/vk_types.h"
#include "renderer/src/renderer.h"
#include "renderer/src/material.h"

#define MAX_MESH_COUNT 512
#define MAX_MATERIAL_BLUEPRINT_COUNT 8
#define MAX_MATERIAL_COUNT 256
#define MAX_IMAGE_COUNT 128

// Big index buffer and vertex buffer
// Add bindless functionality 
// Remove regular

struct image_manager {
    renderer_state* state;
    
    image images[MAX_IMAGE_COUNT];
    u32 image_count;
};

struct material_manager {
    renderer_state* state;
    ds_allocator ds_allocator;
    material_blueprint blueprint;
    
    material materials[MAX_MATERIAL_COUNT];
    u32 material_count;
};

struct mesh_manager {
    renderer_state* state;
    VkCommandPool upload_pool;
    VkFence* upload_fences;
    buffer* staging_buffers;
    u32 upload_count;

    mesh meshes[MAX_MESH_COUNT];
    u32 mesh_count;
};