#pragma once
#include "defines.h"
#include "math/math_types.h"
#include "resources/material.h"

#define MAX_DRAW_COMMANDS 8192
#define MAX_OBJECTS 8192

typedef enum debug_view_type {
    DEBUG_VIEW_TYPE_OFF = 0,
    DEBUG_VIEW_TYPE_SHADOW,
    DEBUG_VIEW_TYPE_METAL_ROUGH,
    DEBUG_VIEW_TYPE_NORMAL,
    DEBUG_VIEW_TYPE_MAX,
} debug_view_type;

typedef struct point_light {
	v4s color;
	v4s position;
} point_light;

typedef struct direction_light {
	v4s color;
	v4s direction;
} direction_light;

typedef struct scene_data {
    // Camera/View
    m4s view;
    m4s proj;
    m4s viewproj;
    v4s view_pos;

    // TEMP: Eventually define multiple lights
    v4s ambient_color;
    point_light light;

    m4s sun_viewproj;   // For shadow mapping
    direction_light sun;
    // TEMP: END
    
    // Kinda hacky spaghetti placement of this info
    float alpha_cutoff;
    u32 max_draw_count;
    u32 shadow_draw_id;
    u32 shadow_map_id;
    u32 debug_view;
} scene_data;