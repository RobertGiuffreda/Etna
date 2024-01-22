#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

layout(location = 0) out vec2 out_uv;
layout(location = 1) out vec4 out_color;

struct vertex {
    vec2 position;
    vec2 uv;
    vec4 color;
};

layout (buffer_reference, std430) readonly buffer vertex_buffer {
    vertex vertices[];
};

layout (push_constant) uniform constants {
    vertex_buffer v_buffer;
} push_constants;

// TODO: GLSL_EXT_buffer_reference2 look into

void main() {
    vertex v = push_constants.v_buffer.vertices[gl_VertexIndex];
    gl_Position = vec4(v.position, 0, 0);
    out_uv = v.uv;
    out_color = v.color;
}