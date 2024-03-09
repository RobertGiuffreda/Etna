#include "camera.h"

#include "core/logger.h"
#include "core/events.h"
#include "core/input.h"

static b8 camera_on_key_event(u16 code, void* cam, event_data data);
static b8 camera_on_mouse_move(u16 code, void* cam, event_data data);

/** NOTE:
 * Delta time needs to be passed to the camera
 * Currently the frame rate is tied to the speed of everything
 */

void camera_create(camera* camera) {
    camera->pitch = 0.0f;
    camera->yaw = 0.0f;
    camera->position = (v3s){.raw = {0.f, 0.f, 0.f}};
    camera->velocity = (v3s){.raw = {0.f, 0.f, 0.f}};

    event_observer_register(EVENT_CODE_KEY_PRESS, camera, camera_on_key_event);
    event_observer_register(EVENT_CODE_KEY_RELEASE, camera, camera_on_key_event);
    event_observer_register(EVENT_CODE_MOUSE_MOVE, camera, camera_on_mouse_move);
}

void camera_destroy(camera* camera) {
    event_observer_deregister(EVENT_CODE_MOUSE_MOVE, camera, camera_on_mouse_move);
    event_observer_deregister(EVENT_CODE_KEY_RELEASE, camera, camera_on_key_event);
    event_observer_deregister(EVENT_CODE_KEY_PRESS, camera, camera_on_key_event);

    camera->velocity = (v3s){.raw = {0.f, 0.f, 0.f}};
    camera->position = (v3s){.raw = {0.f, 0.f, 0.f}};
    camera->yaw = 0.0f;
    camera->pitch = 0.0f;
}

m4s camera_get_view_matrix(camera* camera) {
    m4s camera_translation = glms_translate_make(camera->position);
    m4s camera_rotation = camera_get_rotation_matrix(camera);
    return glms_mat4_inv(glms_mat4_mul(camera_translation, camera_rotation));
}

m4s camera_get_rotation_matrix(camera* camera) {
    versors pitch_quaternion = glms_quatv(camera->pitch, (v3s){.raw = {1.f, 0.f, 0.f}});
    versors yaw_quaternion = glms_quatv(camera->yaw, (v3s){.raw = {0.f, -1.f, 0.f}});
    return glms_mat4_mul(glms_quat_mat4(yaw_quaternion), glms_quat_mat4(pitch_quaternion));
}

void camera_update(camera* camera, f64 dt) {
    m4s cam_rotation = camera_get_rotation_matrix(camera);
    camera->position = glms_vec3_add(camera->position, glms_mat4_mulv3(cam_rotation, glms_vec3_scale(camera->velocity, dt), 0.f));
}

// TODO: Move camera event changes to camera owner structs, like scene
#define CAM_SPEED 10.0f

b8 camera_on_key_event(u16 code, void* cam, event_data data) {
    keys key = EVENT_DATA_KEY(data);
    camera* c = (camera*)cam;

    switch (code)
    {
    case EVENT_CODE_KEY_PRESS:
        switch (key)
        {
        case KEY_W:
            c->velocity.z = -CAM_SPEED;
            break;
        case KEY_S:
            c->velocity.z = CAM_SPEED;
            break;
        case KEY_A:
            c->velocity.x = -CAM_SPEED;
            break;
        case KEY_D:
            c->velocity.x = CAM_SPEED;
            break;
        }
        break;
    case EVENT_CODE_KEY_RELEASE:
        switch (key)
        {
        case KEY_W:
            c->velocity.z = 0.f;
            break;
        case KEY_S:
            c->velocity.z = 0.f;
            break;
        case KEY_A:
            c->velocity.x = 0.f;
            break;
        case KEY_D:
            c->velocity.x = 0.f;
            break;
        }
    }
    return false;
}

b8 camera_on_mouse_move(u16 code, void* cam, event_data data) {
    i32 previous_x, previous_y;
    input_get_previous_mouse_position(&previous_x, &previous_y);
    i32 current_x = EVENT_DATA_MOUSE_X(data);
    i32 current_y = EVENT_DATA_MOUSE_Y(data);

    i32 x_offset = current_x - previous_x;
    i32 y_offset = current_y - previous_y;

    camera* c = (camera*)cam;

    c->yaw += (f32)x_offset/200.f;
    c->pitch -= (f32)y_offset/200.f;

    c->pitch = glm_clamp(c->pitch, glm_rad(-89.f), glm_rad(89.f));
    return false;
}