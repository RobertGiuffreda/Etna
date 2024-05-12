#extension GL_GOOGLE_include_directive : require

#include "../input_structures.glsl"
// NOTE: Push constant elements used:
// buffer0: Joint Matrix buffer (joint transform * inverse bind matrix)
// buffer1: Vertex buffer for input
// buffer2: Vertex buffer for output
// uint0: Number of vertices in vertex buffer we are skinning

layout(buffer_reference, std430) readonly buffer joints {
    uint indices[];
};

// Morph:
// |P0P1P2N0N1N2|P0P1P2N0N1N2|
//     vert1        vert2
layout(buffer_reference, std430) readonly buffer float_buffer {
    float floats[];
};

// TODO: Reduce push constant size for cache friendliness
layout(push_constant) uniform block {
    uint64_t joints;
    uint64_t targets;
    uint64_t weights;
    uint in_vertex_start;
    uint out_vertex_start;
    uint vertex_count;
    uint morph_stride;
    uint morph_count;
} animation_push;