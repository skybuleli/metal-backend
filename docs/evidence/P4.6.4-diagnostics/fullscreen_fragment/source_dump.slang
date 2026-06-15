struct FullscreenVertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

[shader("vertex")]
FullscreenVertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0),
    };

    float2 uv[3] = {
        float2(0.0, 1.0),
        float2(2.0, 1.0),
        float2(0.0, -1.0),
    };

    FullscreenVertexOutput output;
    output.position = float4(positions[vertexId], 0.0, 1.0);
    output.uv = uv[vertexId];
    return output;
}

Texture2D<float4> colorTexture;
SamplerState colorSampler;

[shader("fragment")]
float4 fragmentMain(FullscreenVertexOutput input) : SV_Target0
{
    return colorTexture.Sample(colorSampler, input.uv);
}
