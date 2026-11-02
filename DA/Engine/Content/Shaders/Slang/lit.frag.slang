
struct FrameData
{
    float4x4 view;
    float4x4 proj;
    float4 cameraPos;
    float time;
    float _p0;
    float _p1;
    float _p2;
};

struct Push
{
    float4x4 model;
    float4 baseColor;
    float4 emissive;
    float metallic;
    float roughness;
    float alphaCutoff;
    uint flags;
};

// Frame data: set 0, binding 0
[[vk::binding(0, 0)]]
ConstantBuffer<FrameData> frame;

// Push constants
[[vk::push_constant]]
ConstantBuffer<Push> pc;

// Material textures: set 1, bindings 0-4
[[vk::combinedImageSampler]]
[[vk::binding(0, 1)]]
Sampler2D uBaseColor;

[[vk::combinedImageSampler]]
[[vk::binding(1, 1)]]
Sampler2D uNormal;

[[vk::combinedImageSampler]]
[[vk::binding(2, 1)]]
Sampler2D uMetalRough;

[[vk::combinedImageSampler]]
[[vk::binding(3, 1)]]
Sampler2D uOcclusion;

[[vk::combinedImageSampler]]
[[vk::binding(4, 1)]]
Sampler2D uEmissive;

// Constants
static const uint FLAG_NORMAL_MAP = 1u;
static const uint FLAG_ALPHA_MASK = 2u;
static const float PI = 3.14159265;

// Fragment input
struct PSInput
{
    [[vk::location(0)]] float3 vWorldPos;
    [[vk::location(1)]] float3 vNormal;
    [[vk::location(2)]] float2 vUV;
    [[vk::location(3)]] float3 vColor;
};

// Normal mapping using screen-space derivatives.
// No vertex tangents required.
float3 applyNormalMap(float3 N, float3 worldPos, float2 uv)
{
    float3 tn = uNormal.Sample(uv).xyz * 2.0 - 1.0;

    float3 q1 = ddx(worldPos);
    float3 q2 = ddy(worldPos);

    float2 st1 = ddx(uv);
    float2 st2 = ddy(uv);

    float3 T = normalize(q1 * st2.y - q2 * st1.y);
    float3 B = -normalize(cross(N, T));

    return normalize(T * tn.x + B * tn.y + N * tn.z);
}

// Fragment shader
float4 main(
    PSInput input,
    bool isFrontFace : SV_IsFrontFace
) : SV_Target0
{
    // Base color
    float4 base =
        uBaseColor.Sample(input.vUV) *
        pc.baseColor *
        float4(input.vColor, 1.0);

    // Alpha masking
    if ((pc.flags & FLAG_ALPHA_MASK) != 0u &&
        base.a < pc.alphaCutoff)
    {
        discard;
    }

    // Surface normal
    float3 N = normalize(input.vNormal);

    // Double-sided materials
    if (!isFrontFace)
        N = -N;

    // Normal mapping
    if ((pc.flags & FLAG_NORMAL_MAP) != 0u)
        N = applyNormalMap(N, input.vWorldPos, input.vUV);

    // Metallic / Roughness
    // glTF: G = roughness, B = metallic
    float3 mr = uMetalRough.Sample(input.vUV).rgb;

    float roughness = clamp(
        pc.roughness * mr.g,
        0.05,
        1.0
    );

    float metallic = clamp(
        pc.metallic * mr.b,
        0.0,
        1.0
    );

    // Ambient occlusion
    float ao = uOcclusion.Sample(input.vUV).r;

    // Emissive
    float3 emissive =
        uEmissive.Sample(input.vUV).rgb *
        pc.emissive.rgb;

    // Lighting vectors
    float3 V = normalize(frame.cameraPos.xyz - input.vWorldPos);
    float3 L = normalize(float3(0.4, 1.0, 0.3));
    float3 H = normalize(V + L);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 1e-4);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // GGX Normal Distribution
    float a = roughness * roughness;
    float a2 = a * a;

    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    float D = a2 / (PI * d * d);

    // Smith Geometry
    float k = (roughness + 1.0) * (roughness + 1.0) / 8.0;

    float G =
        (NdotL / (NdotL * (1.0 - k) + k)) *
        (NdotV / (NdotV * (1.0 - k) + k));

    // Fresnel-Schlick
    float3 F0 = lerp(
        float3(0.04, 0.04, 0.04),
        base.rgb,
        metallic
    );

    float3 F = F0 + (1.0 - F0) * pow(1.0 - VdotH, 5.0);

    // Specular BRDF
    float3 specular =
        D * G * F /
        (4.0 * NdotV * max(NdotL, 1e-4));

    // Diffuse BRDF
    float3 diffuse =
        (1.0 - F) *
        (1.0 - metallic) *
        base.rgb / PI;

    // Direct lighting
    float3 radiance = float3(3.0, 3.0, 3.0);

    // Ambient lighting
    float3 ambient =
        0.3 * ao *
        (base.rgb * (1.0 - metallic) + F0 * 0.5);

    // Final lighting
    float3 color =
        (diffuse + specular) * radiance * NdotL +
        ambient +
        emissive;

    // Reinhard tone mapping
    color = color / (color + 1.0);

    return float4(color, 1.0);
}
