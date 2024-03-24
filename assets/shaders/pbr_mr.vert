#version 460
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

layout(set = 1, binding = 0) readonly buffer pbr_draws_buffer {
    draw_command pbr_draws[];
};

struct pbr_inst {
	vec4 color_factors;
	uint color_index;
    float metalness;
    float roughness;
	uint metal_rough_index;
    uint normal_index;
};
layout(set = 1, binding = 1) readonly buffer mat_inst_buffer {
	pbr_inst mat_insts[];
};

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;
layout (location = 3) out vec2 out_uv;
layout (location = 4) flat out uint out_mat_id;
layout (location = 5) flat out uint out_color_id;
layout (location = 6) flat out uint out_mr_id;
layout (location = 7) flat out uint out_normal_id;

void main() {
    draw_command draw = pbr_draws[gl_DrawID];
    vertex v = vertices[gl_VertexIndex];
    mat4 model = transforms[draw.transform_id];

    gl_Position = frame_data.viewproj * model * vec4(v.position, 1.0f);

    out_position = (model * vec4(v.position, 1.0f)).xyz;

    // TODO: Compute the normal matrix on the CPU and not GPU
    out_normal = (transpose(inverse(model)) * vec4(v.normal, 0.0f)).xyz;

    out_color = v.color.rgb * mat_insts[nonuniformEXT(draw.material_id)].color_factors.rgb;
    out_uv.x = v.uv_x;
    out_uv.y = v.uv_y;

    out_mat_id = draw.material_id;
    out_color_id = mat_insts[nonuniformEXT(draw.material_id)].color_index;
    out_mr_id = mat_insts[nonuniformEXT(draw.material_id)].metal_rough_index;
    out_normal_id = mat_insts[nonuniformEXT(draw.material_id)].normal_index;
}