#pragma once
#include "renderer/src/vk_types.h"

void GLTF_MR_build_pipelines(GLTF_MR* mat, renderer_state* state);
void GLTF_MR_destroy_pipelines(GLTF_MR* mat, renderer_state* state);

material_instance GLTF_MR_write_material(
    GLTF_MR* mat,
    renderer_state* state,
    material_pass pass,
    const struct material_resources* resources,
    ds_allocator_growable* descriptor_allocator);