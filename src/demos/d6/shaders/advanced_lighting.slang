struct SceneUniforms
{
    float4x4 viewProj;
    float4x4 lightViewProj;
    float3 lightDir;
    float exposure;
    float3 cameraPos;
    float bloomStrength;
};

struct ObjectUniforms
{
    float4x4 modelMatrix;
    float4x4 modelViewProj;
    float4x4 lightMvp;
    float4 baseColor;
    float4 emissiveColor;
};

ConstantBuffer<SceneUniforms> sceneUniforms;
ConstantBuffer<ObjectUniforms> objectUniforms;
Texture2D<float> shadowTexture;
SamplerComparisonState shadowSampler;

struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
};

struct ShadowOutput
{
    float4 position : SV_Position;
};

struct SceneOutput
{
    float4 position : SV_Position;
    float3 worldPos : TEXCOORD0;
    float3 worldNormal : TEXCOORD1;
    float4 shadowPos : TEXCOORD2;
};

ShadowOutput shadowVertex(VertexInput input)
{
    ShadowOutput output;
    output.position = mul(objectUniforms.lightMvp, float4(input.position, 1.0));
    return output;
}

SceneOutput sceneVertex(VertexInput input)
{
    SceneOutput output;
    float4 localPos = float4(input.position, 1.0);
    float4 worldPos = mul(objectUniforms.modelMatrix, localPos);
    output.position = mul(objectUniforms.modelViewProj, localPos);
    output.worldPos = worldPos.xyz;
    output.worldNormal = normalize(mul((float3x3)objectUniforms.modelMatrix, input.normal));
    output.shadowPos = mul(objectUniforms.lightMvp, localPos);
    return output;
}

float4 sceneFragment(SceneOutput input) : SV_Target0
{
    float3 shadowCoord = input.shadowPos.xyz / input.shadowPos.w;
    float2 shadowUv = shadowCoord.xy * 0.5 + 0.5;
    shadowUv.y = 1.0 - shadowUv.y;

    float shadow = 1.0;
    if (input.shadowPos.w > 0.0 &&
        shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
        shadowUv.y >= 0.0 && shadowUv.y <= 1.0 &&
        shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0)
    {
        shadow = shadowTexture.SampleCmpLevelZero(shadowSampler, shadowUv, shadowCoord.z - 0.0018);
    }

    float3 normal = normalize(input.worldNormal);
    float3 lightDir = normalize(-sceneUniforms.lightDir);
    float3 viewDir = normalize(sceneUniforms.cameraPos - input.worldPos);
    float3 halfDir = normalize(lightDir + viewDir);

    float diffuse = max(dot(normal, lightDir), 0.0);
    float specular = pow(max(dot(normal, halfDir), 0.0), 48.0);
    float rim = pow(1.0 - max(dot(normal, viewDir), 0.0), 3.0) * 0.12;

    float3 ambientColor = objectUniforms.baseColor.rgb * 0.16;
    float3 diffuseColor = objectUniforms.baseColor.rgb * diffuse * shadow;
    float3 specularColor = float3(0.30, 0.32, 0.36) * specular * shadow;
    float3 rimColor = objectUniforms.baseColor.rgb * rim;
    float3 color = ambientColor + diffuseColor + specularColor + rimColor + objectUniforms.emissiveColor.rgb;
    return float4(color, 1.0);
}
