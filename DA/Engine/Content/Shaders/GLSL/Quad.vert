#version 450

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inUV;
layout(location = 0) out vec3 vColor;
layout(location = 1) out vec2 vUV;

layout(set = 0, binding = 0) uniform FrameData {
    float time;
    float aspect;
    vec2 _pad;
} frame;

layout(push_constant) uniform Push {
    vec2 offset;
    float angle;
    float _pad;
} pc;

void main() {
    float c = cos(pc.angle);
    float s = sin(pc.angle);
    vec2 p = vec2(c * inPosition.x - s * inPosition.y, s * inPosition.x + c * inPosition.y);
    p.x /= frame.aspect;
    gl_Position = vec4(p + pc.offset, 0.0, 1.0);
    vColor = inColor;
    vUV = inUV;
}