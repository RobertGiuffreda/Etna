#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require

// NOTE: Information about shader writing in a way the engine will understand
// PUSH_CONSTANT: In use by the engine.
// SET 1: Hardcoded per frame scene & draw call data.
// SET 2: Defined by shader writer to create materials.

layout(set = 0, binding = 0) uniform scene_data_block {
    mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambient_color;
	vec4 light_color;
    vec4 light_position;
	vec4 view_pos;
} scene_data;

// NOTE: struct VkDrawIndexedIndirectCommand parameters
struct draw_command {
	uint index_count;
	uint instance_count;
	uint first_index;
	int vertex_offset;
	uint first_instance;

	uint material_id;
	uint transform_id;
	uint padding;
};
layout(set = 0, binding = 1) readonly buffer draw_commands {
	draw_command draws[];
};

struct vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};
layout(buffer_reference, std430) readonly buffer vertex_buffer {
    vertex v;
};
layout(buffer_reference, std430) readonly buffer transform_buffer {
	mat4 t;
};
// NOTE: Global push constants
layout (push_constant) uniform constants {
    vertex_buffer v_buffer;
	transform_buffer t_buffer;
} push_constants;

// SET 1: Material descriptors
layout(set = 1, binding = 0) uniform material_data_block {
    vec4 color_factors;
    vec4 metal_rough_factors;
} material_data[];
layout(set = 1, binding = 1) uniform sampler2D color_textures[];
layout(set = 1, binding = 2) uniform sampler2D metal_rough_textures[];