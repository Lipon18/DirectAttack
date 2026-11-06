#version 450

layout(location = 0) out vec3 vDir;

layout(set = 0, binding = 0) uniform FrameData {
    mat4 view;
    mat4 proj;
    vec4 cameraPos;
    float time;
    float _p0;
    float _p1;
    float _p2;
} frame;

void main() {
    // Fullscreen triangle from the vertex index: (0,0) (2,0) (0,2).
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 clip = uv * 2.0 - 1.0;
    gl_Position = vec4(clip, 1.0, 1.0);

    // Clip space -> view space -> world direction (rotation only, so the sky follows the camera).
    vec4 viewPos = inverse(frame.proj) * vec4(clip, 1.0, 1.0);
    vDir = transpose(mat3(frame.view)) * (viewPos.xyz / viewPos.w);
}