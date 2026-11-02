#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inNormal;
layout(location = 2) in vec2 inUV;
layout(location = 3) in vec3 inColor;
layout(location = 0) out vec3 vWorldPos;
layout(location = 1) out vec3 vNormal;
layout(location = 2) out vec2 vUV;
layout(location = 3) out vec3 vColor;

layout(set = 0, binding = 0) uniform FrameData {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    float time;
    float _p0;
    float _p1;
    float _p2;
} frame;

layout(push_constant) uniform Push {
    mat4 model;
    vec4 baseColor;
    vec4 emissive;
    float metallic;
    float roughness;
    float alphaCutoff;
    uint flags;
} pc;

void main() {
    vec4 world = pc.model * vec4(inPosition, 1.0);
    vWorldPos = world.xyz;
    vNormal = mat3(pc.model) * inNormal; // exact for rotation + uniform scale only
    vUV = inUV;
    vColor = inColor;
    gl_Position = frame.proj * frame.view * world;
}