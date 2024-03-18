#pragma once
#include "defines.h"
#include "importer_types.h"

b8 import_gltf_payload(import_payload* payload, const char* path);

// NOTE: Outputs the read json data from the glb file out to the specified file
// The file is created if it doesn't exist
b8 dump_gltf_json(const char* gltf_path, const char* dump_file_path);