#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vColor;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameData {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    float time;
    float _p0;
    float _p1;
    float _p2;
} frame;

layout(set = 1, binding = 0) uniform sampler2D uBaseColor;
layout(set = 1, binding = 1) uniform sampler2D uNormal;
layout(set = 1, binding = 2) uniform sampler2D uMetalRough;
layout(set = 1, binding = 3) uniform sampler2D uOcclusion;
layout(set = 1, binding = 4) uniform sampler2D uEmissive;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 baseColor;
    vec4 emissive;
    float metallic;
    float roughness;
    float alphaCutoff;
    uint flags;
} pc;

const uint FLAG_NORMAL_MAP = 1u;
const uint FLAG_ALPHA_MASK = 2u;
const float PI = 3.14159265;

// Tangent frame from screen-space derivatives (no vertex tangents needed).
// If bumps look inverted, flip the sign of B.
vec3 applyNormalMap(vec3 N) {
    vec3 tn = texture(uNormal, vUV).xyz * 2.0 - 1.0;
    vec3 q1 = dFdx(vWorldPos);
    vec3 q2 = dFdy(vWorldPos);
    vec2 st1 = dFdx(vUV);
    vec2 st2 = dFdy(vUV);
    vec3 T = normalize(q1 * st2.t - q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    return normalize(mat3(T, B, N) * tn);
}

void main() {
    vec4 base = texture(uBaseColor, vUV) * pc.baseColor * vec4(vColor, 1.0);
    if ((pc.flags & FLAG_ALPHA_MASK) != 0u && base.a < pc.alphaCutoff) discard;

    vec3 N = normalize(vNormal);
    if (!gl_FrontFacing) N = -N; // double-sided materials
    if ((pc.flags & FLAG_NORMAL_MAP) != 0u) N = applyNormalMap(N);

    vec3 mr = texture(uMetalRough, vUV).rgb; // glTF: G = roughness, B = metallic
    float roughness = clamp(pc.roughness * mr.g, 0.05, 1.0);
    float metallic = clamp(pc.metallic * mr.b, 0.0, 1.0);
    float ao = texture(uOcclusion, vUV).r;
    vec3 emissive = texture(uEmissive, vUV).rgb * pc.emissive.rgb;

    vec3 V = normalize(frame.cameraPos.xyz - vWorldPos);
    vec3 L = normalize(vec3(0.4, 1.0, 0.3));
    vec3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    float a = roughness * roughness;
    float a2 = a * a;
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * d * d);
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;
    float G = (NdotL / (NdotL * (1.0 - k) + k)) * (NdotV / (NdotV * (1.0 - k) + k));
    vec3 F0 = mix(vec3(0.04), base.rgb, metallic);
    vec3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    vec3 specular = D * G * F / (4.0 * NdotV * max(NdotL, 1e-4));
    vec3 diffuse = (1.0 - F) * (1.0 - metallic) * base.rgb / PI;
    vec3 radiance = vec3(3.0);

    vec3 ambient = 0.3 * ao * (base.rgb * (1.0 - metallic) + F0 * 0.5);
    vec3 color = (diffuse + specular) * radiance * NdotL + ambient + emissive;
    color = color / (color + 1.0); // Reinhard; the sRGB swapchain encodes on write
    outColor = vec4(color, 1.0);
}