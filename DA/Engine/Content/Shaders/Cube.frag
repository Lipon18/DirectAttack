#version 450

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 0) out vec4 outColor;

layout(set = 0, binding = 0) uniform FrameData {
    mat4 view;
    mat4 proj;
    float time;
    float _pad0;
    float _pad1;
    float _pad2;
} frame;

layout(set = 0, binding = 1) uniform sampler2D uTexture;

void main() {
    float pulse = 0.85 + 0.15 * sin(frame.time * 3.0);
    vec3 texel = texture(uTexture, vUV).rgb;
    outColor = vec4(texel * mix(vec3(1.0), vColor, 0.35) * pulse, 1.0);
}