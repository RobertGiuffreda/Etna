#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_EXT_shader_explicit_arithmetic_types_int64 : require 

// Buffer to record counts per pipeline shader object
layout(set = 0, binding = 0) uniform frame_data_block {
	mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 view_pos;
	vec4 ambient_color;
	vec4 light_color;
    vec4 light_position;
	uint max_draw_count;
} frame_data;

layout(set = 0, binding = 1, std430) buffer draw_counts {
	uint counts[];
};

struct draw_command {
	uint index_count;
	uint instance_count;
	uint first_index;
	int vertex_offset;
	uint first_instance;

	uint material_id;
	uint transform_id;
};
layout(buffer_reference, std430) writeonly buffer draw_buffer {
	draw_command draws[];
};

// Record draw commands per pipeline shader object
layout(set = 0, binding = 2, std430) readonly buffer draw_buffs {
	uint64_t draw_buffers[];
};

// One for each instance of geometry within a scene, with a material & transform
struct object {
	uint pso_id;		// PipelineShaderObject, one per shader/material
	uint mat_id;		// Material Instance
	uint geo_id;		// Geometry
	uint transform_id;	// Transform
};
layout(set = 0, binding = 3, std430) readonly buffer object_buffer {
	object objects[];
};

// TODO: Add culling parameters
struct geometry {
	uint start_index;
	uint index_count;
	int vertex_offset;
};
// TODO: END
layout(set = 0, binding = 4, std430) readonly buffer geometry_buffer {
	geometry geometries[];
};

struct vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};
layout(set = 0, binding = 5, std430) readonly buffer vertex_buffer {
	vertex vertices[];
};

layout(set = 0, binding = 6, std430) readonly buffer transform_buffer {
	mat4 transforms[];
};

layout(set = 0, binding = 7) uniform sampler2D textures[];