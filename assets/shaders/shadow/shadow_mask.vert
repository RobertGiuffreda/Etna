#version 460
#extension GL_GOOGLE_include_directive : require

#include "../input_structures.glsl"

layout (location = 0) out vec2 out_uv;
layout (location = 1) flat out uint out_color_id;

void main() {
    read_draw_buffer shadow_draws = read_draw_buffer(draw_buffers[frame_data.shadow_draws_id]);
    draw_command draw = shadow_draws.draws[gl_DrawID];
    vertex v = vertices[gl_VertexIndex];
    mat4 model = transforms[draw.transform_id];

    gl_Position = frame_data.sun_viewproj * model * vec4(v.position, 1.0f);

    out_uv = vec2(v.uv_x, v.uv_y);
    // out_color_id = draw.color_id;
    out_color_id = 0;
}