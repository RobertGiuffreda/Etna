// Essentially the current defined Principled BSDF. 
// Currently shaders must conform to these data names
layout(set = 0, binding = 0) uniform scene_data_head {
    mat4 view;
	mat4 proj;
	mat4 viewproj;
	vec4 ambientColor;
	vec4 sunlightDirection; //w for sun power
	vec4 sunlightColor;
    vec4 padding;
} scene_data;

layout(set = 1, binding = 0) uniform GLTFMaterial_data {
    vec4 color_factors;
    vec4 metal_rough_factors;
} material_data;

layout(set = 1, binding = 1) uniform sampler2D color_tex;
layout(set = 1, binding = 2) uniform sampler2D metal_rough_tex;