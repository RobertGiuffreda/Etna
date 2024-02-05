#include "input.h"

#include "memory/etmemory.h"
#include "core/logger.h"
#include "core/events.h"

typedef struct mouse_state {
    i32 x;
    i32 y;
    u8 buttons[BUTTON_MAX];
} mouse_state;

// Each key in key has 8 bits.
// Least significant bit is a flag for down vs up
// Second least significant bit is a flag for repeating.
// If a key is held down the os will send informatino saying the key is repeating
struct input_state {
    u8 previous_keys[KEY_MAX];
    u8 current_keys[KEY_MAX];
    mouse_state previous_mouse;
    mouse_state current_mouse;
};

static struct input_state* state;

b8 input_initialize(input_state** input_system_state) {
    *input_system_state = etallocate(sizeof(struct input_state), MEMORY_TAG_INPUT);
    etzero_memory(*input_system_state, sizeof(struct input_state));
    state = *input_system_state;
    return true;
}

void input_shutdown(input_state* input_system_state) {
    etfree(input_system_state, sizeof(struct input_state), MEMORY_TAG_INPUT);
    state = 0;
}

void input_update(input_state* input_system_state) {
    etcopy_memory(state->previous_keys, state->current_keys, sizeof(b8) * KEY_MAX);
    etcopy_memory(&state->previous_mouse, &state->current_mouse, sizeof(mouse_state));
}

// Handle window signal functions
/* Action values:
 * KEY_PRESS
 * KEY_RELEASE
 * KEY_REPEAT
 */
void input_process_key(keys key, u8 action) {
    event_data e;
    e.u16[0] = (u16)key;
    switch(action)
    {
    case KEY_RELEASE:
        state->current_keys[key] = KEY_RELEASE;
        event_fire(EVENT_CODE_KEY_RELEASE, e);
        break;
    case KEY_PRESS:
        state->current_keys[key] = KEY_PRESS;
        event_fire(EVENT_CODE_KEY_PRESS, e);
        break;
    case KEY_REPEAT:
        state->current_keys[key] |= KEY_REPEAT;
        event_fire(EVENT_CODE_KEY_REPEAT, e);
        break;
    }
}

void input_process_button(buttons button, b8 pressed) {
    event_data e;
    e.u16[0] = (u16)button;
    if (pressed) {
        event_fire(EVENT_CODE_BUTTON_PRESS, e);
    } else {
        event_fire(EVENT_CODE_BUTTON_RELEASE, e);
    }
}

void input_process_mouse_move(i32 x, i32 y) {
    if (state->current_mouse.x != x || state->current_mouse.y != y) {
        state->current_mouse.x = x;
        state->current_mouse.y = y;
        event_data e;
        e.i32[0] = x;
        e.i32[1] = y;
        event_fire(EVENT_CODE_MOUSE_MOVE, e);
    }
}

void input_process_mouse_wheel(u8 z_delta) {
    event_data e;
    e.u8[0] = z_delta;
    event_fire(EVENT_CODE_MOUSE_WHEEL, e);
}

// Polling functions
b8 input_is_key_down(keys key) {
    return (b8)(state->current_keys[key] & (KEY_PRESS | KEY_REPEAT));
}

b8 input_is_key_up(keys key) {
    return (b8)(state->current_keys[key] & KEY_RELEASE);
}

b8 input_was_key_down(keys key) {
    return (b8)(state->previous_keys[key] & (KEY_PRESS | KEY_REPEAT));
}

b8 input_was_key_up(keys key) {
    return (b8)(state->previous_keys[key] & KEY_RELEASE);
}

b8 input_is_button_down(buttons button) {
    return state->current_mouse.buttons[button] == true;
}

b8 input_is_button_up(buttons button) {
    return state->current_mouse.buttons[button] == false;
}

b8 input_was_button_down(buttons button) {
    return state->previous_mouse.buttons[button] == true;
}

b8 input_was_button_up(buttons button) {
    return state->previous_mouse.buttons[button] == false;
}

void input_get_mouse_position(i32* x, i32* y) {
    *x = state->current_mouse.x;
    *y = state->current_mouse.y;
}

void input_get_previous_mouse_position(i32* x, i32* y) {
    *x = state->previous_mouse.x;
    *y = state->previous_mouse.y;
}