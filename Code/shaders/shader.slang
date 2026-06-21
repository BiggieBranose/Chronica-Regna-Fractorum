struct SpriteInput {
    float3 position : LOCATION0;
    float2 uv       : LOCATION1;
    float4 color    : LOCATION2;
};

struct SpriteOutput {
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct PushConstants {
    float4x4 mvp;
};

[[vk::push_constant]]
PushConstants push;

[shader("vertex")]
SpriteOutput vertMain(SpriteInput input) {
    SpriteOutput output;
    output.position = mul(push.mvp, float4(input.position, 1.0));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

[shader("fragment")]
float4 fragMain(SpriteOutput input) : SV_TARGET {
    return input.color;
}
