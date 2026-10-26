#version 450

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameData {
    float time;
    float aspect;
    vec2 _pad;
} frame;

layout(set = 0, binding = 1) uniform sampler2D uTexture;

void main() {
    float pulse = 0.75 + 0.25 * sin(frame.time * 3.0);
    vec3 texel = texture(uTexture, vUV).rgb;
    // Vertex colors tint the texture lightly so both inputs are visibly working.
    outColor = vec4(texel * mix(vec3(1.0), vColor, 0.35) * pulse, 1.0);
}