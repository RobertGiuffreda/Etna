// TODO: Move to view space calculations
// TODO: v/x --> v * 1/x
#version 460
#extension GL_GOOGLE_include_directive : require

#include "input_structures.glsl"

// NOTE: Much of this is from learnopengl.com's information & code about PBR
// I am still learning.

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
layout (location = 1) in vec3 in_normal;
layout (location = 2) in vec3 in_color;
layout (location = 3) in vec2 in_uv;
layout (location = 4) flat in uint in_mat_id;
layout (location = 5) flat in uint in_color_id;
layout (location = 6) flat in uint in_mr_id;
layout (location = 7) flat in uint in_normal_id;

layout(location = 0) out vec4 out_frag_color;

const vec3 gamma_pow = vec3(1.0f / 2.2f);
const float PI = 3.14159265359;
const float INV_PI = 1 / PI;

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

float distribution_GGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
    return nom / denom;
}

float geometry_schlick_ggx(float NdotV, float roughness) {
    float r = (roughness + 1.0f);
    float k = (r * r) * 0.125;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float geometry_smith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = geometry_schlick_ggx(NdotV, roughness);
    float ggx1 = geometry_schlick_ggx(NdotL, roughness);

    return ggx1 * ggx2;
}

vec3 fresnel_schlick(float cos_theta, vec3 F0) {
    return F0 + (1.0f - F0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

void main() {
    // NOTE: Assume the texture is in non linear color space
    vec3 albedo = pow(texture(textures[nonuniformEXT(in_color_id)], in_uv).rgb, vec3(2.2));
    albedo *= in_color; // in_colors has the color factors and the vertex colors

    // TODO: There is a weird soft whiteness that I am unsure if it is supposed to be present on sponza
    // NOTE: I think it is because of lack of IBL, so ambient specular is just a white color, leaving a sheen over the surface
    vec4 mr_sample = texture(textures[nonuniformEXT(in_mr_id)], in_uv);
    float metallic = mat_insts[nonuniformEXT(in_mat_id)].metalness * mr_sample.b;
    float roughness = mat_insts[nonuniformEXT(in_mat_id)].roughness * mr_sample.g;
    // float metallic = 0;
    // float roughness = 0;
    // NOTE: END
    // TODO: END

    // TODO: Something is wrong with the default normal map & its sampler, weird artifacts
    vec3 N = get_normal_from_map();
    // vec3 N = normalize(in_normal);
    // TODO: END

    vec3 V = normalize(frame_data.view_pos.xyz - in_position);

    // calculate reflectance at normal incidence; if dia-electric (like plastic) use F0 
    // of 0.04 and if it's a metal, use the albedo color as F0 (metallic workflow)
    vec3 F0 = vec3(0.4);
    F0 = mix(F0, albedo, metallic);

    // TODO: Multiple lights
    // reflectance equation

    // calculate per-light radiance
    vec3 L = normalize(frame_data.light_position.xyz - in_position);
    vec3 H = normalize(V + L);
    float dist = length(frame_data.light_position.xyz - in_position);
    float attenuation = 1.0 / (dist * dist);
    vec3 radiance = frame_data.light_color.rgb * frame_data.light_color.w * attenuation;

    // Cook-Torrance BRDF
    float NDF = distribution_GGX(N, H, roughness);
    float G = geometry_smith(N, V, L, roughness);      
    vec3 F = fresnel_schlick(max(dot(H, V), 0.0), F0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * max(dot(N, L), 0.0) + 0.0001; // + 0.0001 to prevent divide by zero
    vec3 specular = numerator / denominator;

    // kS is equal to Fresnel
    vec3 kS = F;
    // for energy conservation, the diffuse and specular light can't
    // be above 1.0 (unless the surface emits light); to preserve this
    // relationship the diffuse component (kD) should equal 1.0 - kS.
    vec3 kD = vec3(1.0) - kS;
    // multiply kD by the inverse metalness such that only non-metals 
    // have diffuse lighting, or a linear blend if partly metal (pure metals
    // have no diffuse light).
    kD *= 1.0 - metallic;

    // scale light by NdotL
    float NdotL = max(dot(N, L), 0.0);

    vec3 Lo = (kD * albedo / PI + specular) * radiance * NdotL;
    // TODO: END

    vec3 ambient = vec3(0.03) * albedo;
    vec3 color = Lo + ambient;

    // HDR tonemapping
    color = color / (color + vec3(1.0));
    // gamma correction
    color = pow(color, gamma_pow);

    out_frag_color = vec4(color, 1.0f);
    // out_frag_color = vec4(N, 1.0f);
    // out_frag_color = vec4(texture(textures[nonuniformEXT(in_normal_id)], in_uv).xyz * 2.0 - 1.0, 1.0f);
    // out_frag_color = mr_sample;
}