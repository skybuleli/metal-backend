struct VertexInput
{
    float2 position;
    float2 uv;
};

struct VertexOutput
{
    float4 position : SV_Position;
    float2 uv : TEXCOORD0;
};

StructuredBuffer<VertexInput> vertices;
Texture2D<float4> colorTexture;
SamplerState colorSampler;

VertexOutput vertexMain(uint vertexId : SV_VertexID)
{
    VertexInput input = vertices[vertexId];

    VertexOutput output;
    output.position = float4(input.position, 0.0, 1.0);
    output.uv = input.uv;
    return output;
}

float4 fragmentMain(VertexOutput input) : SV_Target0
{
    return colorTexture.Sample(colorSampler, input.uv);
}
