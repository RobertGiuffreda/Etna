#extension GL_EXT_buffer_reference : require

// Essentially the current defined Principled BSDF. 
// Currently shaders must conform to these data names
layout(set = 0, binding = 0) uniform scene_data_block {
    mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 lightDirection; // Ignore for now
	vec4 lightColor;
    vec4 lightPosition;
} scene_data;

layout(set = 1, binding = 0) uniform material_data_block {
    vec4 color_factors;
    vec4 metal_rough_factors;
} material_data;

layout(set = 1, binding = 1) uniform sampler2D color_tex;
layout(set = 1, binding = 2) uniform sampler2D metal_rough_tex;

// NOTE: Testing the reflection data and such
// layout(set = 1, binding = 3) uniform test_block {
// 	ivec4 test_ivec[4];
// 	uvec4 test_uvec[4];
// } test;

// layout (set = 1, binding = 4) uniform usampler2D test_tex[2];
// layout (rgba16f, set = 1, binding = 5) uniform image2D image;

// layout (buffer_reference, std430) readonly buffer mat_buffer {
//     mat4 mats[];
// };

// layout (set = 1, binding = 6) uniform ref_block {
// 	mat_buffer mat_ref;
// } ref_test;