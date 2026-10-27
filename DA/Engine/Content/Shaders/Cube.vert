#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUV;

layout(set = 0, binding = 0) uniform FrameData {
    mat4 view;
    mat4 proj;
    float time;
    float _pad0;
    float _pad1;
    float _pad2;
} frame;

layout(push_constant) uniform Push {
    mat4 model;
} pc;

void main() {
    gl_Position = frame.proj * frame.view * pc.model * vec4(inPosition, 1.0);
    vColor = inColor;
    vUV = inUV;
}