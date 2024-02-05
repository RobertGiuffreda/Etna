#pragma once

#include "defines.h"

typedef struct application_t application_t;

typedef struct application_config {
    b8 (*initialize)(application_t* app);

    void (*shutdown)(application_t* app);

    b8 (*update)(application_t* app);

    b8 (*render)(application_t* app);

    u64 app_size;
} application_config;