struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

Texture2D<float4> colorTexture;
SamplerState colorSampler;

VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    float2 positions[6] = {
        float2(-0.8,  0.8),
        float2(-0.8, -0.8),
        float2( 0.8, -0.8),
        float2(-0.8,  0.8),
        float2( 0.8, -0.8),
        float2( 0.8,  0.8),
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
    return colorTexture.Sample(colorSampler, input.uv);
}
