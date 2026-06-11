struct UniformBufferObject
{
    float4x4 model;
    float4x4 view;
    float4x4 proj;
};

[[vk::binding(0, 0)]]
ConstantBuffer<UniformBufferObject> ubo;

[[vk::binding(1, 0)]]
Texture2D g_texture;
[[vk::binding(2, 0)]]
SamplerState g_sampler;

struct VSInput
{
    float2 inPosition;
    float3 inColor;
    float2 inTexCoord;
};

struct VSOutput
{
    float4 pos      : SV_Position;
    float3 color    : COLOR0;
    float2 texCoord : TEXCOORD0;
};

[shader("vertex")]
VSOutput vertMain(VSInput input)
{
    VSOutput output;
    output.pos      = mul(ubo.proj, mul(ubo.view, mul(ubo.model, float4(input.inPosition, 0.0, 1.0))));
    output.color    = input.inColor;
    output.texCoord = input.inTexCoord;
    return output;
}

[shader("fragment")]
float4 fragMain(VSOutput vertIn) : SV_TARGET
{
    float4 texColor = g_texture.Sample(g_sampler, vertIn.texCoord);
    return float4(texColor.rgb, 1.0);
}
