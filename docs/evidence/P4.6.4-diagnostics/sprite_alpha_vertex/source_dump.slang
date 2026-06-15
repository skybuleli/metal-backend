struct SpriteSceneData
{
    float4x4 clipFromWorld;
    float4 uvScaleBias;
    float4 globalTint;
};

ConstantBuffer<SpriteSceneData> sceneData;

struct SpriteVertexInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct SpriteVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

[shader("vertex")]
SpriteVertexOutput vertexMain(SpriteVertexInput input)
{
    SpriteVertexOutput output;
    float4 worldPos = float4(input.position, 0.0, 1.0);
    output.position = mul(sceneData.clipFromWorld, worldPos);
    output.uv = input.uv * sceneData.uvScaleBias.xy + sceneData.uvScaleBias.zw;
    output.color = input.color * sceneData.globalTint;
    return output;
}

Texture2D<float4> spriteTexture;
SamplerState spriteSampler;

[shader("fragment")]
float4 fragmentMain(SpriteVertexOutput input) : SV_Target0
{
    float4 texel = spriteTexture.Sample(spriteSampler, input.uv);
    float4 color = texel * input.color;

    if (color.a <= 0.0001)
    {
        discard;
    }

    return color;
}
