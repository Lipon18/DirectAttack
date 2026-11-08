#version 450

layout(location = 0) in vec3 vWorldPos;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec2 vUV;
layout(location = 3) in vec3 vColor;
layout(location = 0) out vec4 outColor;

const int MAX_LIGHTS = 8;

struct Light {
    vec4 positionRadius;
    vec4 colorInner;
    vec4 directionOuter;
};

layout(set = 0, binding = 0) uniform FrameData {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    vec4 misc;        // x = time, y = dynamic light count
    vec4 sunDir;      // xyz = direction TOWARD the sun
    vec4 sunColor;    // rgb = color * intensity
    vec4 ambient;
    vec4 fogColor;    // rgb
    vec4 fogParams;   // x = start, y = end (end <= start disables fog)
    Light lights[MAX_LIGHTS];
} frame;

layout(set = 1, binding = 0) uniform sampler2D uBaseColor;
layout(set = 1, binding = 1) uniform sampler2D uNormal;
layout(set = 1, binding = 4) uniform sampler2D uEmissive;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 baseColor;
    vec4 emissive;
    float metallic;   // unused by this shader, kept so the push layout stays shared
    float roughness;
    float alphaCutoff;
    uint flags;
} pc;

const uint FLAG_NORMAL_MAP = 1u;
const uint FLAG_ALPHA_MASK = 2u;
const uint FLAG_UNLIT = 4u;
const uint FLAG_NO_FOG = 8u;

// Tangent frame from screen-space derivatives (no vertex tangents needed).
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
    // Baked lighting from Blender lives in the vertex color and multiplies the texture here.
    vec4 base = texture(uBaseColor, vUV) * pc.baseColor * vec4(vColor, 1.0);
    if ((pc.flags & FLAG_ALPHA_MASK) != 0u && base.a < pc.alphaCutoff) discard;

    vec3 emissive = texture(uEmissive, vUV).rgb * pc.emissive.rgb;
    vec3 color;

    if ((pc.flags & FLAG_UNLIT) != 0u) {
        color = base.rgb + emissive;
    } else {
        vec3 N = normalize(vNormal);
        if (!gl_FrontFacing) N = -N;
        if ((pc.flags & FLAG_NORMAL_MAP) != 0u) N = applyNormalMap(N);

        vec3 light = frame.ambient.rgb + frame.sunColor.rgb * max(dot(N, frame.sunDir.xyz), 0.0);

        int count = min(int(frame.misc.y), MAX_LIGHTS);
        for (int i = 0; i < count; ++i) {
            Light L = frame.lights[i];
            vec3 toLight = L.positionRadius.xyz - vWorldPos;
            float dist = length(toLight);
            float radius = L.positionRadius.w;
            if (dist >= radius) continue;

            vec3 dir = toLight / dist;
            float att = 1.0 - dist / radius; // smooth: reaches exactly 0 at the radius
            att *= att;

            float spot = 1.0;
            float cosOuter = L.directionOuter.w;
            if (cosOuter > -1.0) { // spot light
                float c = dot(-dir, L.directionOuter.xyz);
                spot = smoothstep(cosOuter, L.colorInner.w, c);
            }
            light += L.colorInner.rgb * max(dot(N, dir), 0.0) * att * spot;
        }
        color = base.rgb * light + emissive;
    }

    // Linear distance fog, the classic way to hide draw distance.
    if ((pc.flags & FLAG_NO_FOG) == 0u && frame.fogParams.y > frame.fogParams.x) {
        float d = length(frame.cameraPos.xyz - vWorldPos);
        float f = clamp((d - frame.fogParams.x) / (frame.fogParams.y - frame.fogParams.x), 0.0, 1.0);
        color = mix(color, frame.fogColor.rgb, f);
    }

    outColor = vec4(color, base.a); // the sRGB swapchain encodes on write; no tonemapping needed
}