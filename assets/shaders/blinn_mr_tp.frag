// TODO: Move to view space calculations
// TODO: v/x --> v * 1/x
#version 460
#extension GL_GOOGLE_include_directive : require

// NOTE: This shader is just blinn phong without using metal roughness at the moment

#include "input_structures.glsl"

layout(set = 1, binding = 0) readonly buffer blinn_draws_buffer {
    draw_command blinn_draws[];
};

struct blinn_inst {
	vec4 color_factors;
	uint color_index;
    float metalness;
    float roughness;
	uint metal_rough_index;
};
layout(set = 1, binding = 1) readonly buffer mat_inst_buffer {
	blinn_inst mat_insts[];
};

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_uv;
layout (location = 4) flat in uint in_color_id;

layout(location = 0) out vec4 out_frag_color;

const vec3 specular_color = vec3(0.3f, 0.3f, 0.3f);
const float shininess = 32;
const float screen_gamma = 2.2;

void main() {
    // Ambient lighting color & power gotten from the scene data
    vec3 ambient = frame_data.ambient_color.rgb * frame_data.ambient_color.w;

    // In color is the vertex colors applied to the texture
    vec4 tex_base_color = texture(textures[nonuniformEXT(in_color_id)], in_uv);
    vec3 diffuse_color = in_color * tex_base_color.rgb;

    // From fragment position to light position direction
    vec3 light_dir = frame_data.light_position.xyz - in_position;
    float dist = length(light_dir);
    float attenuation = 1 / (dist * dist);
    light_dir = normalize(light_dir);
    
    // Normalize the normal
    vec3 normal = normalize(in_normal);

    // Lambertian diffuse factor * diffuse color
    float lambertian = max(dot(light_dir, normal), 0.0f);
    vec3 diffuse = lambertian * diffuse_color;

    // From fragment position to view position direction
    vec3 view_dir = normalize(frame_data.view_pos.xyz - in_position);
    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(normal, halfway_dir), 0.0f), shininess);

    vec3 specular = specular_color * spec; // assuming bright white light color

    vec3 color_linear = (ambient * attenuation) + 
        (diffuse * frame_data.light_color.rgb * frame_data.light_color.w * attenuation) +
        (specular * frame_data.light_color.rgb * frame_data.light_color.w * attenuation);

    vec3 gamma_corrected = pow(color_linear, vec3(1.0f / screen_gamma));

    out_frag_color = vec4(gamma_corrected, 1.0f);
}