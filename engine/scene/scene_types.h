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

// TEMP: Just to get things to work for now, better later
typedef struct {
    u32 mesh;
    i32 vertex_offset;
} skin_mesh_inst;
typedef struct {
    u32 skin;
    u64 joints_addr;
    buffer joints_buffer;
    skin_mesh_inst* meshes; // dynarray
} skin_instance;

typedef struct animation_push {
    u64 joints_addr;
    u64 targets_addr;
    u64 weights_addr;
    u32 in_vertex_start;
    u32 out_vertex_start;
    u32 vertex_count;
    u32 morph_stride;
    u32 morph_count;
} animation_push;
// TEMP: END