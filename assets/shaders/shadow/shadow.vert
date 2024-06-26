#version 460
#extension GL_GOOGLE_include_directive : require

#include "../input_structures.glsl"

// NOTE: This is the shadow map vertex shader for non alpha mask shader

void main() {
    read_draw_buffer shadow_draws = read_draw_buffer(draw_buffers[frame_data.shadow_draws_id]);
    draw_command draw = shadow_draws.draws[gl_DrawID];
    vertex v = vertices[gl_VertexIndex];
    mat4 model = transforms[draw.transform_id];

    gl_Position = frame_data.sun_viewproj * model * vec4(v.position, 1.0f);
}