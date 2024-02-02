#pragma once

#include <defines.h>
#include <math/math_types.h>

struct application_state_t;

b8 application_initialize(struct application_state_t* state);

void application_shutdown(struct application_state_t* state);

b8 application_update(struct application_state_t* state);

b8 application_render(struct application_state_t* state);

u64 get_appstate_size(void);