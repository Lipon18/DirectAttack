#version 450

layout(location = 0) in vec3 vDir;
layout(location = 0) out vec4 outColor;

layout(set = 1, binding = 0) uniform samplerCube uSky;

layout(push_constant) uniform Push {
    vec4 baseColor;
    vec4 fogColor;
    float blend;
    float fogLimitsX;
    float fogLimitsY;
    float _pad;
} pc;

void main() {
    vec3 dir = normalize(vDir);
    vec3 color = texture(uSky, dir).rgb * pc.baseColor.rgb * pc.blend;

    // Optional horizon fog: only active when the two limits differ (the default is -10000, -10000).
    if (pc.fogLimitsX != pc.fogLimitsY) {
        float f = clamp((dir.y - pc.fogLimitsX) / (pc.fogLimitsY - pc.fogLimitsX), 0.0, 1.0);
        color = mix(pc.fogColor.rgb, color, f);
    }
    outColor = vec4(color, 1.0);
}