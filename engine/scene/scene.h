#pragma once
#include "defines.h"
#include "renderer/renderer_types.h"

// TEMP: Eventually scene will deserialize binary format created from import payload
#include "resources/importers/importer_types.h"
// TEMP: END

typedef struct scene scene;

// typedef struct sampler_config sampler_config;
// typedef struct texture_config texture_config;
// typedef struct mesh_config mesh_config;

// typedef struct scene_payload {
//     mat_pipe_config* pipeline_configs;
//     image2D_config* image_configs;
//     sampler_config* sampler_configs;
//     texture_config* texture_configs;
//     mesh_config* mesh_configs;
//     transforms transforms;
//     // animations animations;
// } scene_payload;

typedef struct scene_config {
    const char* name;
    u32 resolution_width;
    u32 resolution_height;
    import_payload* import_payload;
    renderer_state* renderer_state;
} scene_config;

b8 scene_init(scene** scn, scene_config config);

void scene_update(scene* scene, f64 dt);

b8 scene_frame_begin(scene* scene, renderer_state* state);

b8 scene_frame_end(scene* scene, renderer_state* state);

b8 scene_render(scene* scene, renderer_state* state);

void scene_shutdown(scene* scene);