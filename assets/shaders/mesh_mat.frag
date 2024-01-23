#version 450
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_uv;

layout(location = 0) out vec4 out_frag_color;

void main() {

    vec3 color = in_color * texture(color_tex, in_uv).rgb;
    vec3 ambient = color * scene_data.ambient_color.rgb;

    vec3 light_dir = normalize(scene_data.light_position - vec4(in_position, 0.0f)).xyz;
    
    // TODO: normalize needed??
    vec3 normal = normalize(in_normal);

    float diff = max(dot(light_dir, normal), 0.0f);
    vec3 diffuse = diff * color;

    vec3 view_pos = -vec3(scene_data.view[0][3], scene_data.view[1][3], scene_data.view[2][3]);
    vec3 view_dir = normalize(view_pos - in_position);
    vec3 reflect_dir = reflect(-light_dir, normal);

    float spec = 0.0f;

    vec3 halfway_dir = normalize(light_dir + view_dir);
    spec = pow(max(dot(normal, halfway_dir), 0.0f), 32.0f);

    vec3 specular = vec3(0.3) * spec; // assuming bright white light color

    out_frag_color = vec4(ambient + diffuse + specular, 0.0f);
}