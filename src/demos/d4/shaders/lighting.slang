struct VSOutput
{
    float4 position  : SV_Position;
    float3 worldPos  : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
};

static const float3 positions[6] = {
    float3(-0.8, -0.8,  0.0),
    float3( 0.8, -0.8,  0.0),
    float3( 0.8,  0.8,  0.0),
    float3(-0.8, -0.8,  0.0),
    float3( 0.8,  0.8,  0.0),
    float3(-0.8,  0.8,  0.0),
};

VSOutput vertexMain(uint vertexId : SV_VertexID)
{
    float3 pos = positions[vertexId];

    VSOutput output;
    output.position = float4(pos, 1.0);
    output.worldPos = pos;
    output.worldNormal = float3(0.0, 0.0, 1.0);
    return output;
}

float4 fragmentMain(VSOutput input) : SV_Target0
{
    float3 N = normalize(input.worldNormal);
    float3 lightDir = float3(2.0, 3.0, 2.0);
    float3 viewPos = float3(2.0, 2.0, 2.0);
    float3 L = normalize(lightDir - input.worldPos);
    float3 V = normalize(viewPos - input.worldPos);
    float3 R = reflect(-L, N);

    float3 ambient  = float3(0.15, 0.15, 0.18);
    float3 diffuse  = float3(0.7, 0.6, 0.5) * max(dot(N, L), 0.0);
    float3 specular = float3(1.0, 1.0, 1.0) * pow(max(dot(R, V), 0.0), 32.0);

    return float4(ambient + diffuse + specular, 1.0);
}
