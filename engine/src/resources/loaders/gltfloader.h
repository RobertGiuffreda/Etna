#pragma once

#include "defines.h"

b8 load_gltf(struct loaded_gltf* gltf, const char* path, struct renderer_state* state);

void unload_gltf(struct loaded_gltf* gltf);

b8 import_gltf(struct scene* scene, const char* path, struct renderer_state* state);

// NOTE: Outputs the read json data from the glb file out to the specified file
// If the file doesn't exist it is created.
b8 dump_gltf_json(const char* gltf_path, const char* dump_file_path);