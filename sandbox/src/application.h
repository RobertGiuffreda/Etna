#pragma once

#include <defines.h>

struct application_state_t {
    f32 delta_time;
};

b8 application_initialize(struct application_state_t* state);

void application_shutdown(struct application_state_t* state);

b8 application_update(struct application_state_t* state);

b8 application_render(struct application_state_t* state);