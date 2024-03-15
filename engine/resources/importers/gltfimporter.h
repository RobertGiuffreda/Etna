#pragma once
#include "defines.h"
#include "importer_types.h"
#include "renderer/renderer_types.h"
#include "scene/scene.h"

b8 import_gltf(scene* scene, const char* path, renderer_state* state);
import_payload import_gltfs_payload(u32 file_count, const char* const* paths);

// NOTE: Outputs the read json data from the glb file out to the specified file
// If the file doesn't exist it is created.
b8 dump_gltf_json(const char* gltf_path, const char* dump_file_path);