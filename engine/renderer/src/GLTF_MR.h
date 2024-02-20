#pragma once
#include "renderer/src/vk_types.h"

b8 GLTF_MR_build_blueprint(GLTF_MR* mat, renderer_state* state);
void GLTF_MR_destroy_blueprint(GLTF_MR* mat, renderer_state* state);

material_instance GLTF_MR_create_instance(
    GLTF_MR* mat,
    renderer_state* state,
    material_pass pass,
    const struct GLTF_MR_material_resources* resources,
    ds_allocator* descriptor_allocator);