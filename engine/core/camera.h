#pragma once

#include "defines.h"

#include "math/math_types.h"

// TODO: Pass delta_time to the update function of camera so the
// movement of the camera is frame-time independent

typedef struct camera {
    v3s velocity;
    v3s position;

    float pitch;
    float yaw;
} camera;

// Registers the camera for events
void camera_create(camera* camera);
void camera_destroy(camera* camera);
void camera_update(camera* camera, f64 dt);

m4s camera_get_view_matrix(camera* camera);
m4s camera_get_rotation_matrix(camera* camera);