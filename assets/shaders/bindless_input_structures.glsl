#extension GL_EXT_buffer_reference : require
#extension GL_EXT_buffer_reference2 : require
#extension GL_EXT_nonuniform_qualifier : require

// Essentially the current defined Principled BSDF. 
// Currently shaders must conform to these data names
layout(set = 0, binding = 0) uniform scene_data_block {
    mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambient_color;
	vec4 light_color;
    vec4 light_position;
	vec4 view_pos;
} scene_data;

layout(set = 1, binding = 0) uniform material_data_block {
    vec4 color_factors;
    vec4 metal_rough_factors;
} material_data[];

layout(set = 1, binding = 1) uniform sampler2D color_textures[];
layout(set = 1, binding = 2) uniform sampler2D metal_rough_textures[];