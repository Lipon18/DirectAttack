#version 450

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vUV;
layout(location = 2) in vec3 vNormal;
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
    vec3 lightDir = normalize(vec3(0.4, 1.0, 0.3));
    float diffuse = max(dot(normalize(vNormal), lightDir), 0.0);
    float light = 0.25 + 0.75 * diffuse;

    vec3 texel = texture(uTexture, vUV).rgb;
    outColor = vec4(texel * mix(vec3(1.0), vColor, 0.35) * light, 1.0);
}