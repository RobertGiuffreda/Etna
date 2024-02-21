#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require


// Essentially the current defined Principled BSDF. 
layout(set = 0, binding = 0) uniform scene_data_block {
    mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambient_color;
	vec4 light_color;
    vec4 light_position;
	vec4 view_pos;
} scene_data;

struct vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};
layout(buffer_reference, std430) readonly buffer vertices {
    vertex v;
};

struct material {
    uint color_index;
    uint metal_rough_index;
    uint constants_index;
};

struct draw_command {
    uint index_count;
    uint instance_count;
    uint first_index;
    int vertex_offset;
    uint first_instance;
    
    // TODO: Establish method of getting material indices to index into texture arrays and such
};
layout(set = 0, binding = 0) readonly buffer draw_commands {
    draw_commmands draws[];
};

struct object {
    mat4 transform;
    vertices v;
};
layout(set = 0, binding = 0) readonly buffer objects {
    object objects[];
};

layout(set = 1, binding = 1) uniform material_data_block {
    vec4 color_factors;
    vec4 metal_rough_factors;
} material_data;

layout(set = 1, binding = 2) uniform sampler2D color_tex;
layout(set = 1, binding = 3) uniform sampler2D metal_rough_tex;