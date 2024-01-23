#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;
layout (location = 3) out vec2 out_uv;

struct vertex {
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer vertex_buffer {
    vertex vertices[];
};

layout (push_constant) uniform constants {
    mat4 render_matrix;
    vertex_buffer v_buffer;
} push_constants;

void main() {
    vertex v = push_constants.v_buffer.vertices[gl_VertexIndex];

    vec4 position = vec4(v.position, 1.0f);

    gl_Position = scene_data.viewproj * push_constants.render_matrix * position;

    out_position = (push_constants.render_matrix * vec4(v.position, 0.0f)).xyz;
    // TODO: Compute the normal matrix from inverse transpose model view
    out_normal = (push_constants.render_matrix * vec4(v.normal, 0.0f)).xyz;
    out_color = v.color.xyz * material_data.color_factors.xyz;
    out_uv.x = v.uv_x;
    out_uv.y = v.uv_y;
}