#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "renderer/renderer_types.h"

// TEMP: Eventually scene will deserialize binary format created from import payload, then editing of format
#include "resources/importers/importer_types.h"
// TEMP: END

typedef struct scene scene;

b8 scene_init(scene** scn, renderer_state* state, import_payload* payload);

void scene_update(scene* scene, f64 dt);

b8 scene_frame_begin(scene* scene, renderer_state* state);

b8 scene_frame_end(scene* scene, renderer_state* state);

b8 scene_render(scene* scene);

void scene_shutdown(scene* scene);