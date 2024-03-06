#version 460
#extension GL_GOOGLE_include_directive : require

#include "input_structures_refactor.glsl"
// SET 1: Material descriptors

// This needs to be present in each created material
layout(set = 1, binding = 0) readonly buffer blinn_draws_buffer {
    draw_command blinn_draws[];
};

layout(set = 1, binding = 1) uniform blinn_mr_block {
	vec4 color_factors;
    vec4 metal_rough_factors;
	uint color_index;
	uint metal_rough_index;
} blinn_mr[];

layout (location = 0) out vec3 out_position;
layout (location = 1) out vec3 out_normal;
layout (location = 2) out vec3 out_color;
layout (location = 3) out vec2 out_uv;
layout (location = 4) flat out uint out_color_id;

void main() {
    draw_command draw = blinn_draws[gl_DrawID];
    vertex v = vertices[gl_VertexIndex];
    mat4 model = transforms[draw.transform_id];

    gl_Position = frame_data.viewproj * model * vec4(v.position, 1.0f);

    out_position = (model * vec4(v.position, 1.0f)).xyz;

    // TODO: Compute the normal matrix on the CPU and not GPU
    out_normal = (transpose(inverse(model)) * vec4(v.normal, 0.0f)).xyz;
    // out_normal = (push_constants.render_matrix * vec4(v.normal, 0.0f)).xyz;

    out_color = v.color.xyz * blinn_mr[nonuniformEXT(draw.material_id)].color_factors.xyz;
    out_uv.x = v.uv_x;
    out_uv.y = v.uv_y;

    out_color_id = blinn_mr[nonuniformEXT(draw.material_id)].color_index;
}