#pragma once

#include "defines.h"
#include "application_types.h"

typedef struct engine_config {
    i32 x_start_pos;
    i32 y_start_pos;

    i32 width;
    i32 height;

    const char* scene_path;
} engine_config;

b8 engine_initialize(engine_config engine_details, application_config app_details);

b8 engine_run(void);

void engine_shutdown(void);