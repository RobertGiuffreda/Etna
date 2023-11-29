#pragma once
#include "defines.h"

typedef struct etwindow_config {
    const char* name;
    i32 x_start_pos;
    i32 y_start_pos;
    u32 width;
    u32 height;
} etwindow_config;

typedef struct etwindow_state etwindow_state;

b8 etwindow_initialize(etwindow_config* config, etwindow_state** out_window_state);

void etwindow_shutdown(etwindow_state* window_state);

b8 etwindow_should_close(etwindow_state* window_state);

void etwindow_pump_messages(void);