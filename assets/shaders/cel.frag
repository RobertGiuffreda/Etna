#version 460
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

struct cel_inst {
	vec4 color_factors;
	uint color_index;
};
layout(set = 1, binding = 1) readonly buffer mat_inst_buffer {
	cel_inst mat_insts[];
};

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_uv;
layout (location = 4) flat in uint in_color_id;

layout(location = 0) out vec4 out_frag_color;

// TODO: Load from the light or material instance
const vec3 specular_color = vec3(0.3f, 0.3f, 0.3f);

// TODO: Load from material instance
const float shininess = 32;

// TODO: Make configurable via uniforms and GUI
const uint cel_levels = 2;
const float cel_factor = 1.f / cel_levels;
// TODO: END

const vec3 gamma_pow = vec3(1.0f / 2.2f);

void main() {
    vec3 diffuse_color = in_color * texture(textures[nonuniformEXT(in_color_id)], in_uv).rgb;

    vec3 light_dir = frame_data.light_position.xyz - in_position;
    float dist = length(light_dir);
    float attenuation = 1 / (dist * dist);
    light_dir = normalize(light_dir);

    vec3 normal = normalize(in_normal);

    float lambertian = max(dot(light_dir, normal), 0.0f);
    lambertian = ceil(lambertian * cel_levels) * cel_factor;
    vec3 diffuse = lambertian * diffuse_color;

    vec3 view_dir = normalize(frame_data.view_pos.xyz - in_position);
    vec3 halfway_dir = normalize(light_dir + view_dir);

    float spec = pow(max(dot(normal, halfway_dir), 0.0f), shininess);
    spec = ceil(spec * cel_levels) * cel_factor;    

    vec3 specular = specular_color * spec; // assuming bright white light color

    vec3 color_linear = 
        (diffuse * frame_data.light_color.rgb * frame_data.light_color.w * attenuation) +
        (specular * frame_data.light_color.rgb * frame_data.light_color.w * attenuation);

    vec3 gamma_corrected = pow(color_linear, gamma_pow);
    out_frag_color = vec4(gamma_corrected, 1.0f);
}