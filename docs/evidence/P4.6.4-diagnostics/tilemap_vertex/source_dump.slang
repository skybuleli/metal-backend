struct TilemapSceneData
{
    float4x4 clipFromScreen;
    float2 scrollPixels;
    float2 atlasTileSize;
    float2 atlasGridSize;
    float2 mapSizeInTiles;
};

ConstantBuffer<TilemapSceneData> sceneData;

struct TilemapVertexInput
{
    float2 screenPosition : POSITION;
    float2 tileUv : TEXCOORD0;
    float2 tileIndex : TEXCOORD1;
    float4 color : COLOR0;
};

struct TilemapVertexOutput
{
    float4 position : SV_Position;
    float2 atlasUv : TEXCOORD0;
    float4 color : COLOR0;
};

[shader("vertex")]
TilemapVertexOutput vertexMain(TilemapVertexInput input)
{
    TilemapVertexOutput output;
    float2 scrolled = input.screenPosition - sceneData.scrollPixels;
    output.position = mul(sceneData.clipFromScreen, float4(scrolled, 0.0, 1.0));

    float2 atlasBase = input.tileIndex * sceneData.atlasTileSize;
    output.atlasUv = atlasBase + input.tileUv * sceneData.atlasTileSize;
    output.color = input.color;
    return output;
}

Texture2D<float4> tileAtlas;
SamplerState tileSampler;

[shader("fragment")]
float4 fragmentMain(TilemapVertexOutput input) : SV_Target0
{
    float2 atlasUv = input.atlasUv / sceneData.atlasGridSize;
    float4 texel = tileAtlas.Sample(tileSampler, atlasUv);
    return texel * input.color;
}
