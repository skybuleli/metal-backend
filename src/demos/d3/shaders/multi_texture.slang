struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D<float4> primaryTexture;
SamplerState primarySampler;
Texture2D<float4> overlayTexture;
SamplerState overlaySampler;

VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    float2 positions[6] = {
        float2(-0.85,  0.85),
        float2(-0.85, -0.85),
        float2( 0.85, -0.85),
        float2(-0.85,  0.85),
        float2( 0.85, -0.85),
        float2( 0.85,  0.85),
    };
    float2 uvs[6] = {
        float2(0.0, 0.0),
        float2(0.0, 1.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(1.0, 0.0),
    };

    VertexOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = uvs[vertexId];
    return output;
}

float4 fragmentMain(VertexOutput input) : SV_Target0
{
    float4 baseColor = primaryTexture.Sample(primarySampler, input.uv * 6.0);
    float4 overlayColor = overlayTexture.Sample(overlaySampler, input.uv * 1.4 - 0.2);
    float blend = overlayColor.a * 0.65;
    float3 mixedColor = lerp(baseColor.rgb, overlayColor.rgb, blend);
    return float4(mixedColor, 1.0);
}
