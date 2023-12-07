#include "application.h"
#include <application_types.h>

#include <core/etmemory.h>
#include <core/logger.h>
#include <math/etmath.h>
#include <platform/filesystem.h>

struct application_state_t {
    f32 delta_time;
    v4s testing;
};

b8 application_initialize(struct application_state_t* state) {
    // Zero memory allocated for the application state 
    etzero_memory(state, sizeof(struct application_state_t));
    return true;
}

void application_shutdown(struct application_state_t* state) {
}

b8 application_update(struct application_state_t* state) {
    ETINFO("Testing glm vec4s: ");
    ETINFO("v4s.x: %lf | v4s.y: %lf | v4s.z: %lf | v4s.w: %lf.", state->testing.x, state->testing.y, state->testing.z, state->testing.w);
    return true;
}

b8 application_render(struct application_state_t* state) {
    return true;
}

u64 get_appstate_size(void) {
    return sizeof(struct application_state_t);
}