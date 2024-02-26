#pragma once

#include <defines.h>
#include <math/math_types.h>
#include <application_types.h>

b8 application_initialize(application_t* app);

void application_shutdown(application_t* app);

b8 application_update(application_t* app, f64 dt);

b8 application_render(application_t* app);

u64 get_appstate_size(void);