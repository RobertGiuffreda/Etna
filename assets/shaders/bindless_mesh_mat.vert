#version 460
#extension GL_GOOGLE_include_directive : require

#include "input_structures_count.glsl"

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;
layout (location = 3) out vec2 out_uv;
layout (location = 4) flat out uint out_material_id;
layout (location = 5) flat out uint out_color_id;

void main() {
    draw_command draw = draws[gl_DrawID];
    vertex v = push_constants.v_buffer[gl_VertexIndex].v;
    mat4 model = push_constants.t_buffer[draw.transform_id].t;

    gl_Position = scene_data.viewproj * model * vec4(v.position, 1.0f);

    out_position = (model * vec4(v.position, 1.0f)).xyz;

    // TODO: Compute the normal matrix on the CPU and not GPU
    out_normal = (transpose(inverse(model)) * vec4(v.normal, 0.0f)).xyz;
    // out_normal = (push_constants.render_matrix * vec4(v.normal, 0.0f)).xyz;

    out_color = v.color.xyz * material_data[nonuniformEXT(draw.material_id)].color_factors.xyz;
    out_uv.x = v.uv_x;
    out_uv.y = v.uv_y;

    out_material_id = draw.material_id;
    out_color_id = material_data[nonuniformEXT(draw.material_id)].color_index;
}