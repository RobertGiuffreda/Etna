#version 460
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout(set = 1, binding = 0) readonly buffer solid_draws_buffer {
    draw_command solid_draws[];
};

struct solid_inst {
    vec4 color;
};
layout(set = 1, binding = 1) readonly buffer solid_inst_buffer {
	solid_inst mat_insts[];
};

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;
layout (location = 3) out vec2 out_uv;

void main() {
    draw_command draw = solid_draws[gl_DrawID];
    vertex v = vertices[gl_VertexIndex];
    mat4 model = transforms[draw.transform_id];

    gl_Position = frame_data.viewproj * model * vec4(v.position, 1.0f);

    out_position = (model * vec4(v.position, 1.0f)).xyz;

    // TODO: Compute the normal matrix on the CPU and not GPU
    out_normal = (transpose(inverse(model)) * vec4(v.normal, 0.0f)).xyz;
    // out_normal = (push_constants.render_matrix * vec4(v.normal, 0.0f)).xyz;

    out_color = v.color.rgb * mat_insts[draw.material_id].color.rgb;
    out_uv.x = v.uv_x;
    out_uv.y = v.uv_y;
}