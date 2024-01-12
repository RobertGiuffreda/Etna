#pragma once
#include "loader_types.h"

#include "containers/dynarray.h"

// NOTE: This only supports loading .glb files currently

// TODO: Unexpose the renderer functions from this
// Dynarray for mesh_asset returned
struct mesh_asset* load_gltf_meshes(const char* path, struct renderer_state* state);

void load_gltf(struct loaded_gltf* gltf, const char* path, struct renderer_state* state);