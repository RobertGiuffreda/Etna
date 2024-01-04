//glsl version 4.5
#version 450
#extension GL_EXT_buffer_reference : require

// Shader input
layout (location = 0) in vec3 in_color;
layout (location = 1) in vec2 in_uv;

// Output write
layout (location = 0) out vec4 outFragColor;

layout(set = 0, binding = 0) uniform sampler2D display_texture;

void main() {
	//return red
	outFragColor = texture(display_texture, in_uv);
}