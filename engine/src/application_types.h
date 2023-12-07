#pragma once

#include "defines.h"

typedef struct application_state_t application_state;

typedef struct application_config {
    b8 (*initialize)(application_state* state);

    void (*shutdown)(application_state* state);

    b8 (*update)(application_state* state);

    b8 (*render)(application_state* state);

    u64 state_size;
    application_state* state;
} application_config;