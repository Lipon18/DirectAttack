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

[[vk::binding(0, 0)]]
ConstantBuffer<FrameData> frame;

[[vk::push_constant]]
ConstantBuffer<Push> pc;

struct VSInput
{
    [[vk::location(0)]] float3 inPosition;
    [[vk::location(1)]] float3 inNormal;
    [[vk::location(2)]] float2 inUV;
    [[vk::location(3)]] float3 inColor;
};

struct VSOutput
{
    float4 position : SV_Position;

    [[vk::location(0)]] float3 vWorldPos;
    [[vk::location(1)]] float3 vNormal;
    [[vk::location(2)]] float2 vUV;
    [[vk::location(3)]] float3 vColor;
};

VSOutput main(VSInput input)
{
     VSOutput output;

    float4 world = mul(pc.model, float4(input.inPosition, 1.0));

    output.vWorldPos = world.xyz;
    output.vNormal = mul((float3x3)pc.model, input.inNormal);
    output.vUV = input.inUV;
    output.vColor = input.inColor;

    output.position = mul(frame.proj, mul(frame.view, world));

    return output;
}