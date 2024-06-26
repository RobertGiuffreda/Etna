#version 460
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"
#include "common.glsl"

// NOTE: Much of this is from learnopengl.com's information & code about PBR

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

layout (location = 0) in vec3 in_position;
layout (location = 1) in vec4 in_sun_position;
layout (location = 2) in vec3 in_normal;
layout (location = 3) in vec3 in_color;
layout (location = 4) in vec2 in_uv;
layout (location = 5) flat in uint in_mat_id;
layout (location = 6) flat in uint in_color_id;
layout (location = 7) flat in uint in_mr_id;
layout (location = 8) flat in uint in_normal_id;

layout(location = 0) out vec4 out_frag_color;

vec3 get_normal_from_map() {
    vec3 tangent_normal = texture(textures[nonuniformEXT(in_normal_id)], in_uv).xyz * 2.0 - 1.0;

    vec3 Q1 = dFdx(in_position);
    vec3 Q2 = dFdy(in_position);
    vec2 st1 = dFdx(in_uv);
    vec2 st2 = dFdy(in_uv);

    if (length(st1) <= 1e-2) {
        st1 = vec2(1.0, 0.0);
    }

    if (length(st2) <= 1e-2) {
        st2 = vec2(0.0, 1.0);
    }

    vec3 N = normalize(in_normal);
    vec3 T = normalize(Q1*st2.t - Q2*st1.t);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangent_normal);
}

void main() {
    // NOTE: Assume the texture is in non linear color space
    vec4 albedo_sample = texture(textures[nonuniformEXT(in_color_id)], in_uv);
    vec3 albedo = pow(albedo_sample.rgb, GAMMA) * in_color; // in_colors has the color factors and the vertex colors

    vec4 mr_sample = texture(textures[nonuniformEXT(in_mr_id)], in_uv);
    float metallic = clamp(mat_insts[nonuniformEXT(in_mat_id)].metalness * mr_sample.b, 0.0f, 1.0f);
    float roughness = clamp(mat_insts[nonuniformEXT(in_mat_id)].roughness * mr_sample.g, 0.0f, 1.0f);

    vec3 N = get_normal_from_map();
    vec3 V = normalize(frame_data.view_pos.xyz - in_position);

    // calculate reflectance at normal incidence; if dielectric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = vec3(0.04);
    F0 = mix(F0, albedo, metallic);

    // NOTE: Sun/Skylight contribution, no attenuation
    vec3 Ls = normalize(-frame_data.sun.direction.xyz);
    vec3 Hs = normalize(V + Ls);
    vec3 s_radiance = frame_data.sun.color.rgb * frame_data.sun.color.a;
    float NDFs = distribution_ggx(N, Hs, roughness);
    float Gs = geometry_smith(N, V, Ls, roughness);
    vec3 Fs = fresnel_schlick(max(dot(Hs, V), 0.0), F0);

    vec3 s_numerator = NDFs * Gs * Fs;
    float s_denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, Ls), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 s_specular = s_numerator / s_denominator;

    vec3 kSs = Fs;
    vec3 kDs = vec3(1.0) - kSs;
    kDs *= 1.0 - metallic;

    float NdotLs = max(dot(N, Ls), 0.0);

    // TEMP: Create calculate_shadow function
    vec3 shadow_coords = in_sun_position.xyz;
    vec2 shadow_uv = shadow_coords.xy * 0.5f + 0.5f;
    float min_bias_factor = 0.002f;
    float max_bias_factor = 0.005f;
    float bias = max(max_bias_factor * (1.0f - NdotLs), min_bias_factor);

    float shadow = 0.f;
    vec2 texel_size = 1.f / textureSize(textures[frame_data.shadow_map_id], 0);
    for (int x = -1; x <= 1; ++x) {
        for(int y = -1; y <= 1; ++y) {
            float map_depth = texture(textures[frame_data.shadow_map_id], shadow_uv + vec2(x, y) * texel_size).x;
            shadow += (shadow_coords.z + bias < map_depth) ? (1.f / 9.f) : 0.f;
        }
    }

    // float closest_depth = texture(textures[frame_data.shadow_map_id], shadow_uv).x;
    // float current_depth = shadow_coords.z;
    // float shadow = (current_depth + bias < closest_depth) ? 1.0f : 0.0f;

    // https://www.khronos.org/opengl/wiki/Sampler_(GLSL)#Non-uniform_flow_control
    // Alpha discard after all texture sampling has been done to preserve uniform control flow
    if (albedo_sample.a < frame_data.alpha_cutoff) {
        discard;
    }

    vec3 Los = (1.f - shadow) * (kDs * albedo * INV_PI + s_specular) * s_radiance * NdotLs;
    // NOTE: END

    // NOTE: Point light, singular for now
    vec3 L = normalize(frame_data.light.position.xyz - in_position);
    vec3 H = normalize(V + L);
    float dist = length(frame_data.light.position.xyz - in_position);
    float attenuation = 1.0 / (dist * dist);
    vec3 radiance = frame_data.light.color.rgb * frame_data.light.color.a * attenuation;

    // Cook-Torrance BRDF
    float NDF = distribution_ggx(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;

    vec3 kS = F;
    vec3 kD = vec3(1.0) - kS;
    kD *= 1.0 - metallic;

    float NdotL = max(dot(N, L), 0.0);
    vec3 Lo = (kD * albedo * INV_PI + specular) * radiance * NdotL;
    // NOTE: END

    vec3 ambient = vec3(0.03) * albedo * frame_data.ambient_color.a;
    vec3 color = Lo + Los + ambient;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correction
    color = pow(color, INV_GAMMA);

    out_frag_color = vec4(color, 1.0f);
    if (frame_data.debug_view == DEBUG_VIEW_TYPE_SHADOW) {
        out_frag_color = vec4(vec3(1.f - shadow), 1.0f);
    }
    else if (frame_data.debug_view == DEBUG_VIEW_TYPE_METAL_ROUGH) {
        out_frag_color = vec4(shadow_coords.xy, 0.0f, 1.0f);
    }
    else if (frame_data.debug_view == DEBUG_VIEW_TYPE_NORMAL) {
        out_frag_color = vec4(N, 1.0);
    }
}