#version 460
#extension GL_GOOGLE_include_directive : require

#include "../input_structures.glsl"

layout (location = 0) in vec2 in_uv;
layout (location = 1) flat in uint in_color_id;

void main() {
    if (texture(textures[nonuniformEXT(in_color_id)], in_uv).a < frame_data.alpha_cutoff) {
        discard;
    }
}