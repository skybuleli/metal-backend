// D8 Complex Showcase — PBR 材质球 + 阴影 + 天空盒 + 后处理 + 粒子 + HUD + 自由摄像机
// 着色器路径：计算着色器 Path A (Slang→DXIL→MSC→metallib)，渲染着色器 inline MSL
// 场景：7 个 PBR 球体 (不同粗糙度/金属度) + 地面 + 程序化天空盒 + 粒子漩涡

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define IR_RUNTIME_METALCPP
#define IR_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{

// ═══════════════════════════════════════════════════════════════════════
// 常量
// ═══════════════════════════════════════════════════════════════════════
constexpr std::uint32_t kWidth = 960;
constexpr std::uint32_t kHeight = 720;
constexpr std::uint32_t kShadowSize = 1024;
constexpr std::uint32_t kBloomWidth = 480;
constexpr std::uint32_t kBloomHeight = 360;
constexpr std::uint32_t kParticleCount = 4096;
constexpr std::uint32_t kThreadgroupSize = 64;
constexpr std::uint32_t kSampleFrames = 180;
constexpr float kDeltaTime = 1.0f / 60.0f;
constexpr std::uint32_t kSphereSlices = 48;
constexpr std::uint32_t kSphereStacks = 24;
constexpr const char* kComputeMetallibPath = "build/d8_particle_update.metallib";

// ═══════════════════════════════════════════════════════════════════════
// 基础数据结构
// ═══════════════════════════════════════════════════════════════════════
struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

struct Float3
{
    float x;
    float y;
    float z;
};

struct alignas(16) Mat4
{
    float m[16] = {};
};

struct Vertex
{
    float position[3];
    float normal[3];
};

// PBR 场景 uniform（对齐 MSL 的 float4x4 + packed_float3 布局）
struct alignas(16) SceneUniforms
{
    float viewProj[16];          // offset  0: float4x4
    float lightViewProj[16];     // offset 64: float4x4
    float lightDir[3];           // offset 128: packed_float3
    float exposure;              // offset 140
    float cameraPos[3];          // offset 144: packed_float3
    float bloomStrength;         // offset 156
    float sunDir[3];             // offset 160: packed_float3
    float skyIntensity;          // offset 172
    float time;                  // offset 176
    float pad[3];                // offset 180 → 总共 192 字节
};

static_assert(sizeof(SceneUniforms) == 192, "SceneUniforms 大小必须为 192 字节");

// 每个物体的 uniform（对齐 MSL float4x4 + float4 布局）
struct alignas(16) ObjectUniforms
{
    float modelMatrix[16];
    float modelViewProj[16];
    float lightMvp[16];
    float normalMatrix[16];
    float baseColor[4];
    float roughness;
    float metallic;
    float specular;
    float pad;
};

static_assert(sizeof(ObjectUniforms) == 288, "ObjectUniforms 大小必须为 288 字节");

// ═══════════════════════════════════════════════════════════════════════
// 粒子系统结构
// ═══════════════════════════════════════════════════════════════════════
struct alignas(16) Particle
{
    float position[2];
    float velocity[2];
    float color[4];
    float size;
    float age;
    float lifetime;
    float phase;
};

static_assert(sizeof(Particle) == 48, "Particle 大小必须为 48 字节");

struct alignas(16) DrawArgs
{
    std::uint32_t vertexCount;
    std::uint32_t instanceCount;
    std::uint32_t vertexStart;
    std::uint32_t baseInstance;
};

static_assert(sizeof(DrawArgs) == 16, "DrawArgs 大小必须为 16 字节");

struct alignas(16) SimParams
{
    std::uint32_t particleCount;
    float deltaTime;
    float time;
    float aspect;
    std::uint32_t frameIndex;
    float speedScale;
    float drag;
    float verticalLimit;
};

static_assert(sizeof(SimParams) == 32, "SimParams 大小必须为 32 字节");

struct alignas(16) ParticleRenderParams
{
    float aspect;
    float time;
    float intensity;
    float pad;
};

static_assert(sizeof(ParticleRenderParams) == 16, "ParticleRenderParams 大小必须为 16 字节");

struct PerfStats
{
    double avgFps;
    double avgFrameMs;
    double sampleSeconds;
    std::uint32_t minInstanceCount;
    std::uint32_t maxInstanceCount;
    std::uint32_t finalInstanceCount;
};

// ═══════════════════════════════════════════════════════════════════════
// 场景物品定义
// ═══════════════════════════════════════════════════════════════════════
struct SceneItem
{
    Float3 translation;
    Float3 baseColor;
    float roughness;
    float metallic;
    const char* label;     // 材质标签（用于 HUD）
};

// 7 个 PBR 球体，排列成弧形，材质各异
constexpr SceneItem kSceneItems[] = {
    // 位置 X/Y/Z         颜色(RGB)          粗糙度  金属度  标签
    {{-1.80f, -0.30f,  0.60f}, {0.94f, 0.78f, 0.22f}, 0.15f, 1.00f, "A Gold"},         // 黄金
    {{-1.20f,  0.10f, -0.20f}, {0.90f, 0.90f, 0.92f}, 0.05f, 1.00f, "B Chrome"},       // 铬
    {{-0.50f, -0.10f,  0.90f}, {0.88f, 0.45f, 0.22f}, 0.25f, 1.00f, "C Copper"},       // 铜
    {{ 0.10f,  0.20f, -0.60f}, {0.82f, 0.18f, 0.28f}, 0.40f, 0.00f, "D Plastic"},      // 塑料
    {{ 0.70f, -0.05f,  0.40f}, {0.95f, 0.93f, 0.88f}, 0.60f, 0.00f, "E Ceramic"},      // 陶瓷
    {{ 1.30f,  0.15f, -0.35f}, {0.38f, 0.58f, 0.88f}, 0.20f, 0.95f, "F Blue Metal"},   // 蓝色金属
    {{ 1.80f,  0.35f,  0.55f}, {0.30f, 0.86f, 0.42f}, 0.12f, 0.70f, "G Green Metal"},  // 绿色金属
};

// HUD 面板数据结构
struct HudRect
{
    float x;
    float y;
    float w;
    float h;
    float color[3];
    float alpha;
};

// HUD 元素（半透明面板 + 标签条）
constexpr HudRect kHudElements[] = {
    // 顶部面板
    {0.015f, 0.015f, 0.32f, 0.10f, {0.02f, 0.03f, 0.06f}, 0.80f},
    // 材质标签底色
    {0.015f, 0.85f, 0.52f, 0.12f, {0.02f, 0.03f, 0.06f}, 0.75f},
};

// ═══════════════════════════════════════════════════════════════════════
// 顶点数据生成
// ═══════════════════════════════════════════════════════════════════════

// 程序化生成 UV 球体顶点（位置 + 法线），三角形列表
std::vector<Vertex> BuildSphereVertices()
{
    std::vector<Vertex> vertices;
    vertices.reserve(kSphereSlices * kSphereStacks * 6);

    for (std::uint32_t stack = 0; stack < kSphereStacks; ++stack)
    {
        const float phi1 = static_cast<float>(stack) / static_cast<float>(kSphereStacks) * 3.14159265f;
        const float phi2 = static_cast<float>(stack + 1) / static_cast<float>(kSphereStacks) * 3.14159265f;
        const float sinPhi1 = std::sin(phi1);
        const float cosPhi1 = std::cos(phi1);
        const float sinPhi2 = std::sin(phi2);
        const float cosPhi2 = std::cos(phi2);

        for (std::uint32_t slice = 0; slice < kSphereSlices; ++slice)
        {
            const float theta1 = static_cast<float>(slice) / static_cast<float>(kSphereSlices) * 2.0f * 3.14159265f;
            const float theta2 = static_cast<float>(slice + 1) / static_cast<float>(kSphereSlices) * 2.0f * 3.14159265f;
            const float sinTheta1 = std::sin(theta1);
            const float cosTheta1 = std::cos(theta1);
            const float sinTheta2 = std::sin(theta2);
            const float cosTheta2 = std::cos(theta2);

            // 四个顶点
            const Float3 v1 = {sinPhi1 * cosTheta1, cosPhi1, sinPhi1 * sinTheta1};
            const Float3 v2 = {sinPhi1 * cosTheta2, cosPhi1, sinPhi1 * sinTheta2};
            const Float3 v3 = {sinPhi2 * cosTheta2, cosPhi2, sinPhi2 * sinTheta2};
            const Float3 v4 = {sinPhi2 * cosTheta1, cosPhi2, sinPhi2 * sinTheta1};

            // 两个三角形（极区退化为单点三角形，正常行为）
            vertices.push_back({{v1.x, v1.y, v1.z}, {v1.x, v1.y, v1.z}});
            vertices.push_back({{v2.x, v2.y, v2.z}, {v2.x, v2.y, v2.z}});
            vertices.push_back({{v3.x, v3.y, v3.z}, {v3.x, v3.y, v3.z}});

            vertices.push_back({{v1.x, v1.y, v1.z}, {v1.x, v1.y, v1.z}});
            vertices.push_back({{v3.x, v3.y, v3.z}, {v3.x, v3.y, v3.z}});
            vertices.push_back({{v4.x, v4.y, v4.z}, {v4.x, v4.y, v4.z}});
        }
    }
    return vertices;
}

// 地面平面
std::vector<Vertex> BuildPlaneVertices()
{
    return {
        {{-8.0f, -1.20f, -8.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 8.0f, -1.20f, -8.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 8.0f, -1.20f,  8.0f}, {0.0f, 1.0f, 0.0f}},
        {{-8.0f, -1.20f, -8.0f}, {0.0f, 1.0f, 0.0f}},
        {{ 8.0f, -1.20f,  8.0f}, {0.0f, 1.0f, 0.0f}},
        {{-8.0f, -1.20f,  8.0f}, {0.0f, 1.0f, 0.0f}},
    };
}



// HUD 顶点（仅 2D 位置，NDC 坐标）
std::vector<float> BuildHudPositions()
{
    std::vector<float> verts;
    for (const auto& rect : kHudElements)
    {
        const float x0 = rect.x * 2.0f - 1.0f;
        const float y0 = 1.0f - rect.y * 2.0f;
        const float x1 = (rect.x + rect.w) * 2.0f - 1.0f;
        const float y1 = 1.0f - (rect.y + rect.h) * 2.0f;

        const float quad[12] = {x0, y0, x1, y0, x1, y1, x0, y0, x1, y1, x0, y1};
        verts.insert(verts.end(), std::begin(quad), std::end(quad));
    }
    return verts;
}

// ═══════════════════════════════════════════════════════════════════════
// MSL 着色器源码（全部内嵌）
// ═══════════════════════════════════════════════════════════════════════
const char* kSceneShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

// ── 结构体（与 CPU 侧对齐） ──
struct SceneUniforms
{
    float4x4 viewProj;
    float4x4 lightViewProj;
    packed_float3 lightDir;
    float exposure;
    packed_float3 cameraPos;
    float bloomStrength;
    packed_float3 sunDir;
    float skyIntensity;
    float time;
    float3 pad;
};

struct ObjectUniforms
{
    float4x4 modelMatrix;
    float4x4 modelViewProj;
    float4x4 lightMvp;
    float4x4 normalMatrix;
    float4 baseColor;
    float roughness;
    float metallic;
    float specular;
    float pad;
};

struct VertexIn
{
    float3 position [[attribute(0)]];
    float3 normal [[attribute(1)]];
};

struct ShadowOut
{
    float4 position [[position]];
};

struct SceneOut
{
    float4 position [[position]];
    float3 worldPos;
    float3 worldNormal;
    float4 shadowPos;
};

struct QuadOut
{
    float4 position [[position]];
    float2 uv;
};

// ── Shadow Pass ──
vertex ShadowOut shadowVertex(VertexIn in [[stage_in]],
                              constant ObjectUniforms& object [[buffer(1)]])
{
    ShadowOut out;
    out.position = object.lightMvp * float4(in.position, 1.0);
    return out;
}

// ── Scene Pass ──
vertex SceneOut sceneVertex(VertexIn in [[stage_in]],
                            constant ObjectUniforms& object [[buffer(1)]])
{
    SceneOut out;
    float4 worldPos4 = object.modelMatrix * float4(in.position, 1.0);
    out.position = object.modelViewProj * float4(in.position, 1.0);
    out.worldPos = worldPos4.xyz;
    out.worldNormal = normalize((object.normalMatrix * float4(in.normal, 0.0)).xyz);
    out.shadowPos = object.lightMvp * float4(in.position, 1.0);
    return out;
}

// PBR: GGX 分布函数
float GGX_D(float NdotH, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (3.14159265 * denom * denom + 0.00001);
}

// PBR: Smith 几何遮蔽函数
float Smith_G(float NdotV, float NdotL, float roughness)
{
    float a = roughness * roughness;
    float G_V = NdotV + sqrt(NdotV * NdotV * (1.0 - a) + a);
    float G_L = NdotL + sqrt(NdotL * NdotL * (1.0 - a) + a);
    return 1.0 / max(G_V * G_L, 0.00001);
}

// PBR: Schlick Fresnel（f0: 绝缘体反射率 ≈ 0.04，金属用 baseColor）
float3 Schlick_F(float3 f0, float VdotH)
{
    return f0 + (1.0 - f0) * pow(clamp(1.0 - VdotH, 0.0, 1.0), 5.0);
}

fragment float4 sceneFragment(SceneOut in [[stage_in]],
                              constant ObjectUniforms& object [[buffer(0)]],
                              constant SceneUniforms& scene [[buffer(1)]],
                              depth2d<float> shadowTexture [[texture(0)]],
                              sampler shadowSampler [[sampler(0)]])
{
    float3 N = normalize(in.worldNormal);
    float3 L = normalize(-scene.lightDir);
    float3 V = normalize(scene.cameraPos - in.worldPos);
    float3 H = normalize(L + V);

    float NdotL = max(dot(N, L), 0.0);
    float NdotV = max(dot(N, V), 0.0);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);

    // ── PBR 参数 ──
    float roughness = clamp(object.roughness, 0.04, 1.0);
    float metallic = clamp(object.metallic, 0.0, 1.0);
    float3 albedo = object.baseColor.rgb;

    // 金属使用 baseColor 作为 F0，绝缘体使用 0.04
    float3 f0 = mix(float3(0.04), albedo, metallic);

    // ── Cook-Torrance BRDF ──
    float D = GGX_D(NdotH, roughness);
    float G = Smith_G(NdotV, NdotL, roughness);
    float3 F = Schlick_F(f0, VdotH);

    float3 specular = (D * G * F) / max(4.0 * NdotV * NdotL, 0.0001);

    // Disney 风格漫反射（绝缘体有更明显的漫反射）
    float3 kD = (1.0 - F) * (1.0 - metallic);
    float3 diffuse = kD * albedo / 3.14159265;

    // ── 阴影采样 ──
    float3 shadowCoord = in.shadowPos.xyz / in.shadowPos.w;
    float2 shadowUv = shadowCoord.xy * 0.5 + 0.5;
    shadowUv.y = 1.0 - shadowUv.y;
    float shadow = 1.0;
    if (in.shadowPos.w > 0.0 &&
        shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
        shadowUv.y >= 0.0 && shadowUv.y <= 1.0 &&
        shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0)
    {
        constexpr sampler compareSampler(coord::normalized, filter::linear,
                                         address::clamp_to_edge, compare_func::less_equal);
        shadow = shadowTexture.sample_compare(compareSampler, shadowUv, shadowCoord.z - 0.0020);
    }

    // ── 直接光照 ──
    float3 directLight = (diffuse + specular) * NdotL * shadow * 3.0;

    // ── 环境光 ──
    float3 ambient = albedo * mix(0.03, 0.06, metallic) * scene.skyIntensity;
    float3 rim = pow(1.0 - NdotV, 4.0) * f0 * 0.25 * scene.skyIntensity;

    float3 color = ambient + directLight + rim;
    return float4(color, 1.0);
}

// ── 天空盒 (程序化，ray-marched) ──
fragment float4 skyFragment(SceneOut in [[stage_in]],
                            constant SceneUniforms& scene [[buffer(1)]])
{
    float3 rayDir = normalize(in.worldPos - scene.cameraPos);

    // 天空渐变：上深下浅
    float height = clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0);
    float3 skyTop = float3(0.08, 0.12, 0.32);
    float3 skyHorizon = float3(0.62, 0.74, 0.92);
    float3 skyBottom = float3(0.35, 0.45, 0.55);
    float3 skyColor = mix(skyHorizon, skyTop, smoothstep(0.12, 0.40, height));
    skyColor = mix(skyBottom, skyColor, smoothstep(0.0, 0.08, height));

    // 太阳光晕
    float sunDot = max(dot(rayDir, normalize(-scene.sunDir)), 0.0);
    float sunDisc = smoothstep(0.9997, 0.99996, sunDot);
    float sunGlow = pow(sunDot, 480.0) * 0.35;
    float sunHalo = pow(sunDot, 32.0) * 0.08;

    float3 sunColor = float3(1.0, 0.95, 0.75) * (sunDisc + sunGlow + sunHalo);

    return float4(skyColor + sunColor, 1.0);
}

// ── 全屏 Quad Vertex ──
vertex QuadOut quadVertex(uint vertexId [[vertex_id]])
{
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0),
    };
    QuadOut out;
    out.position = float4(positions[vertexId], 0.0, 1.0);
    out.uv = positions[vertexId] * 0.5 + 0.5;
    return out;
}

// ── 后处理：高亮提取 (Bloom) ──
fragment float4 brightExtractFragment(QuadOut in [[stage_in]],
                                      texture2d<float> hdrTexture [[texture(0)]],
                                      sampler linearSampler [[sampler(0)]])
{
    float3 color = hdrTexture.sample(linearSampler, in.uv).rgb;
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float intensity = smoothstep(1.0, 2.5, luminance);
    return float4(color * intensity, 1.0);
}

// ── 后处理：水平模糊 ──
fragment float4 blurHorizontalFragment(QuadOut in [[stage_in]],
                                       texture2d<float> inputTexture [[texture(0)]],
                                       sampler linearSampler [[sampler(0)]])
{
    float2 texel = 1.0 / float2(inputTexture.get_width(), inputTexture.get_height());
    float3 color = inputTexture.sample(linearSampler, in.uv).rgb * 0.2941176;
    color += inputTexture.sample(linearSampler, in.uv + float2(texel.x * 1.5, 0.0)).rgb * 0.2352941;
    color += inputTexture.sample(linearSampler, in.uv - float2(texel.x * 1.5, 0.0)).rgb * 0.2352941;
    color += inputTexture.sample(linearSampler, in.uv + float2(texel.x * 3.5, 0.0)).rgb * 0.1176470;
    color += inputTexture.sample(linearSampler, in.uv - float2(texel.x * 3.5, 0.0)).rgb * 0.1176470;
    return float4(color, 1.0);
}

// ── 后处理：垂直模糊 ──
fragment float4 blurVerticalFragment(QuadOut in [[stage_in]],
                                     texture2d<float> inputTexture [[texture(0)]],
                                     sampler linearSampler [[sampler(0)]])
{
    float2 texel = 1.0 / float2(inputTexture.get_width(), inputTexture.get_height());
    float3 color = inputTexture.sample(linearSampler, in.uv).rgb * 0.2941176;
    color += inputTexture.sample(linearSampler, in.uv + float2(0.0, texel.y * 1.5)).rgb * 0.2352941;
    color += inputTexture.sample(linearSampler, in.uv - float2(0.0, texel.y * 1.5)).rgb * 0.2352941;
    color += inputTexture.sample(linearSampler, in.uv + float2(0.0, texel.y * 3.5)).rgb * 0.1176470;
    color += inputTexture.sample(linearSampler, in.uv - float2(0.0, texel.y * 3.5)).rgb * 0.1176470;
    return float4(color, 1.0);
}

// ── 后处理：Tone Mapping + Bloom 合成 ──
fragment float4 compositeFragment(QuadOut in [[stage_in]],
                                  constant SceneUniforms& scene [[buffer(0)]],
                                  texture2d<float> hdrTexture [[texture(0)]],
                                  texture2d<float> bloomTexture [[texture(1)]],
                                  sampler linearSampler [[sampler(0)]])
{
    float3 hdr = hdrTexture.sample(linearSampler, in.uv).rgb;
    float3 bloom = bloomTexture.sample(linearSampler, in.uv).rgb * scene.bloomStrength;
    float3 color = hdr + bloom;
    // Reinhard tone mapping
    float3 mapped = color / (color + 1.0);
    // Gamma 校正
    mapped = pow(mapped, float3(1.0 / 2.2));
    return float4(mapped, 1.0);
}
)";

// ── 粒子渲染着色器（内嵌 MSL） ──
const char* kParticleShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
    float size;
    float age;
    float lifetime;
    float phase;
};

struct ParticleRenderParams
{
    float aspect;
    float time;
    float intensity;
    float pad;
};

struct ParticleVertexOut
{
    float4 position [[position]];
    float4 color;
    float2 localUv;
};

vertex ParticleVertexOut particleVertex(uint vertexId [[vertex_id]],
                                        uint instanceId [[instance_id]],
                                        const device Particle* particles [[buffer(0)]],
                                        constant ParticleRenderParams& params [[buffer(1)]])
{
    float2 quad[6] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0,  1.0),
    };

    Particle particle = particles[instanceId];
    float life = clamp(1.0 - particle.age / max(particle.lifetime, 0.001), 0.0, 1.0);
    float pulse = 0.65 + 0.35 * sin(params.time * 3.1 + particle.phase * 2.0);
    float size = particle.size * mix(0.75, 1.35, life) * pulse * params.intensity;

    ParticleVertexOut out;
    out.localUv = quad[vertexId];
    float2 offset = quad[vertexId] * float2(size / params.aspect, size);
    out.position = float4(particle.position + offset, 0.0, 1.0);
    out.color = float4(particle.color.rgb * (0.30 + 0.70 * life) * (0.65 + 0.35 * pulse), life);
    return out;
}

fragment float4 particleFragment(ParticleVertexOut in [[stage_in]])
{
    float radius = length(in.localUv);
    float glow = smoothstep(1.0, 0.0, radius);
    float core = smoothstep(0.40, 0.0, radius);
    float alpha = glow * in.color.a;
    float3 color = in.color.rgb * (0.35 + core * 1.25 + glow * 0.40);
    return float4(color * alpha, alpha);
}
)";

// ── HUD 覆盖层着色器（简化：仅位置 + 固定半透明色） ──
const char* kHudShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct HudVertexOut
{
    float4 position [[position]];
};

vertex HudVertexOut hudVertex(uint vertexId [[vertex_id]],
                              const device float2* positions [[buffer(0)]])
{
    HudVertexOut out;
    float2 pos = positions[vertexId];
    out.position = float4(pos, 0.0, 1.0);
    return out;
}

fragment float4 hudFragment(HudVertexOut in [[stage_in]])
{
    return float4(0.02, 0.03, 0.06, 0.82);
}
)";

// ═══════════════════════════════════════════════════════════════════════
// 3D 数学工具函数
// ═══════════════════════════════════════════════════════════════════════
Float3 Add(const Float3& a, const Float3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Float3 Sub(const Float3& a, const Float3& b)
{
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Float3 Scale(const Float3& value, float amount)
{
    return {value.x * amount, value.y * amount, value.z * amount};
}

float Dot(const Float3& a, const Float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Float3 Cross(const Float3& a, const Float3& b)
{
    return {
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x,
    };
}

Float3 Normalize(const Float3& value)
{
    const float length = std::sqrt(std::max(Dot(value, value), 1.0e-8f));
    return {value.x / length, value.y / length, value.z / length};
}

Mat4 Mat4Identity()
{
    Mat4 result;
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[15] = 1.0f;
    return result;
}

Mat4 Mat4Mul(const Mat4& a, const Mat4& b)
{
    Mat4 result;
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.0f;
            for (int index = 0; index < 4; ++index)
            {
                sum += a.m[index * 4 + row] * b.m[col * 4 + index];
            }
            result.m[col * 4 + row] = sum;
        }
    }
    return result;
}

Mat4 Mat4Translate(float x, float y, float z)
{
    Mat4 result = Mat4Identity();
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    return result;
}

Mat4 Mat4Scale(float x, float y, float z)
{
    Mat4 result = {};
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    return result;
}

Mat4 Mat4Perspective(float fovY, float aspect, float nearZ, float farZ)
{
    const float tanHalf = std::tan(fovY * 0.5f);
    Mat4 result = {};
    result.m[0] = 1.0f / (aspect * tanHalf);
    result.m[5] = 1.0f / tanHalf;
    result.m[10] = farZ / (nearZ - farZ);
    result.m[11] = -1.0f;
    result.m[14] = (nearZ * farZ) / (nearZ - farZ);
    return result;
}

Mat4 Mat4Orthographic(float left, float right, float bottom, float top, float nearZ, float farZ)
{
    Mat4 result = Mat4Identity();
    result.m[0] = 2.0f / (right - left);
    result.m[5] = 2.0f / (top - bottom);
    result.m[10] = 1.0f / (nearZ - farZ);
    result.m[12] = (left + right) / (left - right);
    result.m[13] = (top + bottom) / (bottom - top);
    result.m[14] = nearZ / (nearZ - farZ);
    return result;
}

Mat4 Mat4LookAt(Float3 eye, Float3 target, Float3 up)
{
    const Float3 forward = Normalize(Sub(target, eye));
    const Float3 side = Normalize(Cross(forward, up));
    const Float3 trueUp = Cross(side, forward);

    Mat4 result = Mat4Identity();
    result.m[0] = side.x;
    result.m[4] = side.y;
    result.m[8] = side.z;
    result.m[12] = -Dot(side, eye);
    result.m[1] = trueUp.x;
    result.m[5] = trueUp.y;
    result.m[9] = trueUp.z;
    result.m[13] = -Dot(trueUp, eye);
    result.m[2] = -forward.x;
    result.m[6] = -forward.y;
    result.m[10] = -forward.z;
    result.m[14] = Dot(forward, eye);
    return result;
}

Mat4 Mat4Transpose(const Mat4& src)
{
    Mat4 result;
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            result.m[col * 4 + row] = src.m[row * 4 + col];
        }
    }
    return result;
}

Mat4 Mat4Inverse(const Mat4& src)
{
    // 仅用于正交矩阵的逆（模型视图法线矩阵），使用转置
    return Mat4Transpose(src);
}

// ═══════════════════════════════════════════════════════════════════════
// 场景构建函数
// ═══════════════════════════════════════════════════════════════════════

// 自由摄像机（轨道控制）
struct CameraParams
{
    float distance;     // 距目标点距离
    float azimuth;      // 水平角度
    float elevation;    // 仰角
    Float3 target;      // 注视点
};

CameraParams BuildCameraParams(float time)
{
    // 缓慢自动旋转 + 上下浮动
    CameraParams params;
    params.azimuth = time * 0.22f;
    params.elevation = 0.32f + 0.08f * std::sin(time * 0.15f);
    params.distance = 5.8f + 0.6f * std::sin(time * 0.10f);
    params.target = {0.05f, -0.15f, 0.10f};
    return params;
}

Float3 CameraEyePosition(const CameraParams& params)
{
    const float cosEl = std::cos(params.elevation);
    return {
        params.target.x + std::cos(params.azimuth) * cosEl * params.distance,
        params.target.y + std::sin(params.elevation) * params.distance,
        params.target.z + std::sin(params.azimuth) * cosEl * params.distance,
    };
}

Mat4 BuildModelMatrix(const SceneItem& item)
{
    return Mat4Mul(
        Mat4Translate(item.translation.x, item.translation.y, item.translation.z),
        Mat4Scale(0.55f, 0.55f, 0.55f));
}

SceneUniforms BuildSceneUniforms(float time)
{
    const CameraParams camera = BuildCameraParams(time);
    const Float3 eye = CameraEyePosition(camera);
    const float aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);

    const Mat4 view = Mat4LookAt(eye, camera.target, {0.0f, 1.0f, 0.0f});
    const Mat4 proj = Mat4Perspective(3.14159f * 0.22f, aspect, 0.1f, 40.0f);

    // 定向光源（从右上方照射，产生可见阴影）
    const Float3 lightDir = Normalize({-0.55f, -0.78f, -0.30f});
    const Float3 lightEye = Add(camera.target, Scale(lightDir, -10.0f));
    const Mat4 lightView = Mat4LookAt(lightEye, camera.target, {0.0f, 1.0f, 0.0f});
    const Mat4 lightProj = Mat4Orthographic(-8.0f, 8.0f, -8.0f, 8.0f, 0.1f, 24.0f);

    SceneUniforms uniforms = {};
    const Mat4 viewProj = Mat4Mul(proj, view);
    const Mat4 lightViewProj = Mat4Mul(lightProj, lightView);
    std::memcpy(uniforms.viewProj, viewProj.m, sizeof(viewProj.m));
    std::memcpy(uniforms.lightViewProj, lightViewProj.m, sizeof(lightViewProj.m));
    uniforms.lightDir[0] = lightDir.x;
    uniforms.lightDir[1] = lightDir.y;
    uniforms.lightDir[2] = lightDir.z;
    uniforms.exposure = 1.05f;
    uniforms.cameraPos[0] = eye.x;
    uniforms.cameraPos[1] = eye.y;
    uniforms.cameraPos[2] = eye.z;
    uniforms.bloomStrength = 0.55f;
    uniforms.sunDir[0] = -lightDir.x;
    uniforms.sunDir[1] = -lightDir.y;
    uniforms.sunDir[2] = -lightDir.z;
    uniforms.skyIntensity = 0.55f;
    uniforms.time = time;
    return uniforms;
}

ObjectUniforms BuildObjectUniforms(const SceneItem& item, const SceneUniforms& scene)
{
    ObjectUniforms uniforms = {};
    const Mat4 model = BuildModelMatrix(item);

    Mat4 viewProj = {};
    Mat4 lightViewProj = {};
    std::memcpy(viewProj.m, scene.viewProj, sizeof(viewProj.m));
    std::memcpy(lightViewProj.m, scene.lightViewProj, sizeof(lightViewProj.m));

    const Mat4 mvp = Mat4Mul(viewProj, model);
    const Mat4 lightMvp = Mat4Mul(lightViewProj, model);

    std::memcpy(uniforms.modelMatrix, model.m, sizeof(model.m));
    std::memcpy(uniforms.modelViewProj, mvp.m, sizeof(mvp.m));
    std::memcpy(uniforms.lightMvp, lightMvp.m, sizeof(lightMvp.m));

    // 法线矩阵 = (M^-1)^T 的上 3x3 部分
    const Mat4 modelInv = Mat4Inverse(model);
    Mat4 normalMat = Mat4Transpose(modelInv);
    std::memcpy(uniforms.normalMatrix, normalMat.m, sizeof(normalMat.m));

    uniforms.baseColor[0] = item.baseColor.x;
    uniforms.baseColor[1] = item.baseColor.y;
    uniforms.baseColor[2] = item.baseColor.z;
    uniforms.baseColor[3] = 1.0f;
    uniforms.roughness = item.roughness;
    uniforms.metallic = item.metallic;
    uniforms.specular = 0.5f;
    uniforms.pad = 0.0f;
    return uniforms;
}

// ═══════════════════════════════════════════════════════════════════════
// Metal 资源工厂函数
// ═══════════════════════════════════════════════════════════════════════
std::string ErrorToString(NS::Error* error)
{
    if (error == nullptr)
    {
        return "未知错误";
    }
    NS::String* description = error->localizedDescription();
    return description != nullptr ? description->utf8String() : "未知错误";
}

MTL::Buffer* MakeSharedBuffer(MTL::Device* device, const void* bytes, std::size_t size)
{
    return device->newBuffer(bytes, static_cast<NS::UInteger>(size), MTL::ResourceStorageModeShared);
}

MTL::Texture* MakeColorTexture(MTL::Device* device,
                               MTL::PixelFormat pixelFormat,
                               std::uint32_t width,
                               std::uint32_t height,
                               MTL::TextureUsage usage,
                               MTL::StorageMode storageMode)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(pixelFormat, width, height, false);
    descriptor->setUsage(usage);
    descriptor->setStorageMode(storageMode);
    return device->newTexture(descriptor);
}

MTL::Texture* MakeDepthTexture(MTL::Device* device,
                               std::uint32_t width,
                               std::uint32_t height,
                               MTL::TextureUsage usage)
{
    MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
    descriptor->setTextureType(MTL::TextureType2D);
    descriptor->setPixelFormat(MTL::PixelFormatDepth32Float);
    descriptor->setWidth(width);
    descriptor->setHeight(height);
    descriptor->setDepth(1);
    descriptor->setMipmapLevelCount(1);
    descriptor->setSampleCount(1);
    descriptor->setUsage(usage);
    descriptor->setStorageMode(MTL::StorageModePrivate);
    return device->newTexture(descriptor);
}

MTL::Library* LoadMetallib(MTL::Device* device, const char* path)
{
    NS::Error* error = nullptr;
    NS::String* libraryPath = NS::String::string(path, NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(libraryPath, &error);
    if (library == nullptr)
    {
        std::cerr << "无法加载 metallib " << path << ": " << ErrorToString(error) << "\n";
    }
    return library;
}

MTL::VertexDescriptor* CreateVertexDescriptor()
{
    MTL::VertexDescriptor* desc = MTL::VertexDescriptor::alloc()->init();
    desc->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    desc->attributes()->object(0)->setOffset(0);
    desc->attributes()->object(0)->setBufferIndex(0);
    desc->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
    desc->attributes()->object(1)->setOffset(sizeof(float) * 3);
    desc->attributes()->object(1)->setBufferIndex(0);
    desc->layouts()->object(0)->setStride(sizeof(Vertex));
    return desc;
}

// ═══════════════════════════════════════════════════════════════════════
// 管线构建函数
// ═══════════════════════════════════════════════════════════════════════
MTL::RenderPipelineState* BuildShadowPipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertexFn = library->newFunction(MTLSTR("shadowVertex"));
    MTL::VertexDescriptor* vertexDesc = CreateVertexDescriptor();

    MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vertexFn);
    desc->setVertexDescriptor(vertexDesc);
    desc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(desc, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 Shadow 管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildScenePipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertexFn = library->newFunction(MTLSTR("sceneVertex"));
    MTL::Function* fragmentFn = library->newFunction(MTLSTR("sceneFragment"));
    MTL::VertexDescriptor* vertexDesc = CreateVertexDescriptor();

    MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vertexFn);
    desc->setFragmentFunction(fragmentFn);
    desc->setVertexDescriptor(vertexDesc);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
    desc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(desc, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 HDR 场景管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildSkyPipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertexFn = library->newFunction(MTLSTR("sceneVertex"));
    MTL::Function* fragmentFn = library->newFunction(MTLSTR("skyFragment"));
    MTL::VertexDescriptor* vertexDesc = CreateVertexDescriptor();

    MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vertexFn);
    desc->setFragmentFunction(fragmentFn);
    desc->setVertexDescriptor(vertexDesc);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
    desc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(desc, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建天空盒管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildFullscreenPipeline(MTL::Device* device,
                                                  MTL::Library* library,
                                                  const char* fragmentName,
                                                  MTL::PixelFormat colorFormat)
{
    NS::Error* error = nullptr;
    MTL::Function* vertexFn = library->newFunction(MTLSTR("quadVertex"));
    NS::String* fragNameNs = NS::String::string(fragmentName, NS::UTF8StringEncoding);
    MTL::Function* fragmentFn = library->newFunction(fragNameNs);

    MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vertexFn);
    desc->setFragmentFunction(fragmentFn);
    desc->colorAttachments()->object(0)->setPixelFormat(colorFormat);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(desc, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建后处理管线 " << fragmentName << ": " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildParticlePipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertexFn = library->newFunction(MTLSTR("particleVertex"));
    MTL::Function* fragmentFn = library->newFunction(MTLSTR("particleFragment"));

    MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vertexFn);
    desc->setFragmentFunction(fragmentFn);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    desc->colorAttachments()->object(0)->setBlendingEnabled(true);
    desc->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
    desc->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
    desc->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorOne);
    desc->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOne);
    desc->colorAttachments()->object(0)->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    desc->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(desc, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建粒子管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildHudPipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertexFn = library->newFunction(MTLSTR("hudVertex"));
    MTL::Function* fragmentFn = library->newFunction(MTLSTR("hudFragment"));

    MTL::RenderPipelineDescriptor* desc = MTL::RenderPipelineDescriptor::alloc()->init();
    desc->setVertexFunction(vertexFn);
    desc->setFragmentFunction(fragmentFn);
    desc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    desc->colorAttachments()->object(0)->setBlendingEnabled(true);
    desc->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
    desc->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
    desc->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    desc->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(desc, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 HUD 管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::ComputePipelineState* BuildComputePipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* function = library->newFunction(MTLSTR("main"));
    if (function == nullptr)
    {
        std::cerr << "无法从 compute metallib 中取出入口函数 main。\n";
        return nullptr;
    }
    MTL::ComputePipelineState* pipeline = device->newComputePipelineState(function, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 ComputePipelineState: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::DepthStencilState* BuildDepthState(MTL::Device* device, MTL::CompareFunction compare)
{
    MTL::DepthStencilDescriptor* desc = MTL::DepthStencilDescriptor::alloc()->init();
    desc->setDepthCompareFunction(compare);
    desc->setDepthWriteEnabled(true);
    return device->newDepthStencilState(desc);
}

// ═══════════════════════════════════════════════════════════════════════
// I/O 函数
// ═══════════════════════════════════════════════════════════════════════
bool WritePpm(const std::filesystem::path& outputPath, const std::vector<Pixel>& pixels)
{
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        return false;
    }
    output << "P6\n" << kWidth << " " << kHeight << "\n255\n";
    for (std::uint32_t y = 0; y < kHeight; ++y)
    {
        const std::uint32_t srcY = kHeight - 1 - y;
        for (std::uint32_t x = 0; x < kWidth; ++x)
        {
            const Pixel& pixel = pixels[srcY * kWidth + x];
            output.put(static_cast<char>(pixel.r));
            output.put(static_cast<char>(pixel.g));
            output.put(static_cast<char>(pixel.b));
        }
    }
    return true;
}

bool WritePerfJson(const std::filesystem::path& outputPath,
                   const std::string& deviceName,
                   const PerfStats& stats)
{
    std::ofstream output(outputPath);
    if (!output)
    {
        return false;
    }
    output << std::fixed << std::setprecision(4);
    output << "{\n";
    output << "  \"task\": \"P2.10\",\n";
    output << "  \"demo\": \"D8\",\n";
    output << "  \"device\": \"" << deviceName << "\",\n";
    output << "  \"scene\": \"PBR spheres (GGX) + shadow map + procedural skybox + bloom + particles + HUD + free camera\",\n";
    output << "  \"particle_count\": " << kParticleCount << ",\n";
    output << "  \"sample_frames\": " << kSampleFrames << ",\n";
    output << "  \"sample_seconds\": " << stats.sampleSeconds << ",\n";
    output << "  \"avg_fps\": " << stats.avgFps << ",\n";
    output << "  \"avg_frame_ms\": " << stats.avgFrameMs << ",\n";
    output << "  \"render_passes\": [\"shadow\", \"scene(PBR+skybox)\", \"bright_extract\", "
           << "\"blur_h\", \"blur_v\", \"composite\", \"particles\", \"hud\"],\n";
    output << "  \"shader_path\": {\"compute\": \"path-a-metallib\", \"render\": \"handwritten-msl\"},\n";
    output << "  \"pbr_model\": \"Cook-Torrance GGX with Smith geometry and Schlick Fresnel\",\n";
    output << "  \"camera\": \"free orbit (auto-rotate + altitude oscillation)\",\n";
    output << "  \"final_instance_count\": " << stats.finalInstanceCount << "\n";
    output << "}\n";
    return true;
}

// ═══════════════════════════════════════════════════════════════════════
// 粒子初始化
// ═══════════════════════════════════════════════════════════════════════
std::vector<Particle> BuildInitialParticles()
{
    std::vector<Particle> particles(kParticleCount);
    std::mt19937 rng(20260612u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    const float aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
    for (std::uint32_t index = 0; index < kParticleCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(kParticleCount);
        const float angle = t * 6.2831853f * 12.0f + unit(rng) * 0.8f;
        const float radius = 0.18f + 0.95f * std::sqrt(unit(rng));
        const float speed = 0.11f + unit(rng) * 0.13f;

        Particle particle = {};
        particle.position[0] = std::cos(angle) * radius * aspect;
        particle.position[1] = std::sin(angle) * radius;
        particle.velocity[0] = -std::sin(angle) * speed;
        particle.velocity[1] = std::cos(angle) * speed;
        particle.color[0] = 0.25f + 0.75f * unit(rng);
        particle.color[1] = 0.35f + 0.60f * unit(rng);
        particle.color[2] = 0.55f + 0.45f * unit(rng);
        particle.color[3] = 1.0f;
        particle.size = 0.010f + unit(rng) * 0.020f;
        particle.age = unit(rng) * 2.0f;
        particle.lifetime = 1.8f + unit(rng) * 2.8f;
        particle.phase = angle;
        particles[index] = particle;
    }
    return particles;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
// 主入口
// ═══════════════════════════════════════════════════════════════════════
int main()
{
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    // ── 设备与队列创建 ──
    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (device == nullptr)
    {
        std::cerr << "无法创建 MTLDevice。\n";
        pool->drain();
        return 1;
    }
    const std::string deviceName = device->name()->utf8String();
    std::cout << "设备: " << deviceName << "\n";

    MTL::CommandQueue* commandQueue = device->newCommandQueue();
    if (commandQueue == nullptr)
    {
        std::cerr << "无法创建 MTLCommandQueue。\n";
        pool->drain();
        return 1;
    }

    // ── 加载/编译着色器 ──
    MTL::Library* computeLibrary = LoadMetallib(device, kComputeMetallibPath);
    if (computeLibrary == nullptr)
    {
        pool->drain();
        return 1;
    }
    std::cout << "Path A compute metallib 加载通过\n";

    NS::Error* error = nullptr;
    NS::String* sceneSource = NS::String::string(kSceneShaderSource, NS::UTF8StringEncoding);
    MTL::Library* sceneLibrary = device->newLibrary(sceneSource, nullptr, &error);
    if (sceneLibrary == nullptr)
    {
        std::cerr << "场景 MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "场景着色器 (PBR+Skybox+PP) 编译通过\n";

    NS::String* particleSource = NS::String::string(kParticleShaderSource, NS::UTF8StringEncoding);
    MTL::Library* particleLibrary = device->newLibrary(particleSource, nullptr, &error);
    if (particleLibrary == nullptr)
    {
        std::cerr << "粒子 MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "粒子着色器编译通过\n";

    NS::String* hudSource = NS::String::string(kHudShaderSource, NS::UTF8StringEncoding);
    MTL::Library* hudLibrary = device->newLibrary(hudSource, nullptr, &error);
    if (hudLibrary == nullptr)
    {
        std::cerr << "HUD MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "HUD 着色器编译通过\n";

    // ── 管线创建 ──
    MTL::ComputePipelineState* computePipeline = BuildComputePipeline(device, computeLibrary);
    MTL::RenderPipelineState* shadowPipeline = BuildShadowPipeline(device, sceneLibrary);
    MTL::RenderPipelineState* scenePipeline = BuildScenePipeline(device, sceneLibrary);
    MTL::RenderPipelineState* skyPipeline = BuildSkyPipeline(device, sceneLibrary);
    MTL::RenderPipelineState* brightPipeline = BuildFullscreenPipeline(device, sceneLibrary, "brightExtractFragment", MTL::PixelFormatRGBA16Float);
    MTL::RenderPipelineState* blurHPipeline = BuildFullscreenPipeline(device, sceneLibrary, "blurHorizontalFragment", MTL::PixelFormatRGBA16Float);
    MTL::RenderPipelineState* blurVPipeline = BuildFullscreenPipeline(device, sceneLibrary, "blurVerticalFragment", MTL::PixelFormatRGBA16Float);
    MTL::RenderPipelineState* compositePipeline = BuildFullscreenPipeline(device, sceneLibrary, "compositeFragment", MTL::PixelFormatBGRA8Unorm);
    MTL::RenderPipelineState* particlePipeline = BuildParticlePipeline(device, particleLibrary);
    MTL::RenderPipelineState* hudPipeline = BuildHudPipeline(device, hudLibrary);

    if (computePipeline == nullptr || shadowPipeline == nullptr || scenePipeline == nullptr ||
        skyPipeline == nullptr || brightPipeline == nullptr || blurHPipeline == nullptr ||
        blurVPipeline == nullptr || compositePipeline == nullptr || particlePipeline == nullptr ||
        hudPipeline == nullptr)
    {
        std::cerr << "管线创建失败。\n";
        pool->drain();
        return 1;
    }
    std::cout << "10 条管线全部创建成功\n";

    // ── 深度状态 ──
    MTL::DepthStencilState* shadowDepthState = BuildDepthState(device, MTL::CompareFunctionLessEqual);
    MTL::DepthStencilState* sceneDepthState = BuildDepthState(device, MTL::CompareFunctionLess);

    // ── 采样器 ──
    MTL::SamplerDescriptor* linearSamplerDesc = MTL::SamplerDescriptor::alloc()->init();
    linearSamplerDesc->setMinFilter(MTL::SamplerMinMagFilterLinear);
    linearSamplerDesc->setMagFilter(MTL::SamplerMinMagFilterLinear);
    linearSamplerDesc->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
    linearSamplerDesc->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    linearSamplerDesc->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    MTL::SamplerState* linearSampler = device->newSamplerState(linearSamplerDesc);

    if (linearSampler == nullptr)
    {
        std::cerr << "无法创建采样器。\n";
        pool->drain();
        return 1;
    }

    // ── 几何体数据 ──
    const std::vector<Vertex> sphereVertices = BuildSphereVertices();
    const std::vector<Vertex> planeVertices = BuildPlaneVertices();
    const std::vector<float> hudPositions = BuildHudPositions();

    MTL::Buffer* sphereBuffer = MakeSharedBuffer(device, sphereVertices.data(), sphereVertices.size() * sizeof(Vertex));
    MTL::Buffer* planeBuffer = MakeSharedBuffer(device, planeVertices.data(), planeVertices.size() * sizeof(Vertex));
    MTL::Buffer* hudBuffer = MakeSharedBuffer(device, hudPositions.data(), hudPositions.size() * sizeof(float));

    // ── 粒子系统初始化 ──
    std::vector<Particle> initialParticles = BuildInitialParticles();
    DrawArgs initialArgs = {6u, 0u, 0u, 0u};
    SimParams simParams = {
        .particleCount = kParticleCount,
        .deltaTime = kDeltaTime,
        .time = 0.0f,
        .aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight),
        .frameIndex = 0,
        .speedScale = 1.0f,
        .drag = 0.92f,
        .verticalLimit = 1.18f,
    };
    ParticleRenderParams particleRenderParams = {
        .aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight),
        .time = 0.0f,
        .intensity = 1.0f,
        .pad = 0.0f,
    };

    MTL::Buffer* particleBuffer = MakeSharedBuffer(device, initialParticles.data(), initialParticles.size() * sizeof(Particle));
    MTL::Buffer* indirectArgsBuffer = MakeSharedBuffer(device, &initialArgs, sizeof(initialArgs));
    MTL::Buffer* simParamsBuffer = MakeSharedBuffer(device, &simParams, sizeof(simParams));
    MTL::Buffer* particleRenderParamsBuffer = MakeSharedBuffer(device, &particleRenderParams, sizeof(particleRenderParams));

    // ── 纹理创建 ──
    MTL::Texture* shadowTexture = MakeDepthTexture(device, kShadowSize, kShadowSize,
        MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    MTL::Texture* hdrTexture = MakeColorTexture(device, MTL::PixelFormatRGBA16Float, kWidth, kHeight,
        MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
    MTL::Texture* hdrDepth = MakeDepthTexture(device, kWidth, kHeight, MTL::TextureUsageRenderTarget);
    MTL::Texture* brightTexture = MakeColorTexture(device, MTL::PixelFormatRGBA16Float, kBloomWidth, kBloomHeight,
        MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
    MTL::Texture* blurPingTexture = MakeColorTexture(device, MTL::PixelFormatRGBA16Float, kBloomWidth, kBloomHeight,
        MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
    MTL::Texture* blurPongTexture = MakeColorTexture(device, MTL::PixelFormatRGBA16Float, kBloomWidth, kBloomHeight,
        MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
    MTL::Texture* compositeTexture = MakeColorTexture(device, MTL::PixelFormatBGRA8Unorm, kWidth, kHeight,
        MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModeShared);

    constexpr int kItemCount = static_cast<int>(std::size(kSceneItems));
    constexpr int kSphereIndexCount = kSphereSlices * kSphereStacks * 6;
    constexpr int kPlaneIndexCount = 6;

    // ── 渲染循环 ──
    const auto begin = std::chrono::steady_clock::now();
    std::uint32_t minInstanceCount = kParticleCount;
    std::uint32_t maxInstanceCount = 0;
    std::uint32_t finalInstanceCount = 0;

    std::cout << "D8 粒子数: " << kParticleCount << "\n";
    std::cout << "场景物品: " << kItemCount << " 个 PBR 球体 + 地面 + 天空盒\n";
    std::cout << "渲染通道: Shadow → Scene(PBR+Skybox) → Bloom(3pass) → Composite → Particles → HUD\n";
    std::cout << "采样帧数: " << kSampleFrames << "\n";

    for (std::uint32_t frameIndex = 0; frameIndex < kSampleFrames; ++frameIndex)
    {
        const float time = static_cast<float>(frameIndex) * kDeltaTime;

        // 更新模拟参数
        simParams.time = time;
        simParams.frameIndex = frameIndex;
        std::memcpy(simParamsBuffer->contents(), &simParams, sizeof(simParams));

        particleRenderParams.time = time;
        particleRenderParams.intensity = 1.0f + 0.08f * std::sin(time * 1.7f);
        std::memcpy(particleRenderParamsBuffer->contents(), &particleRenderParams, sizeof(particleRenderParams));

        // 构建场景 uniform（含自由摄像机）
        const SceneUniforms sceneUniforms = BuildSceneUniforms(time);
        MTL::Buffer* sceneBuffer = MakeSharedBuffer(device, &sceneUniforms, sizeof(sceneUniforms));

        // 构建物体 uniform
        std::array<MTL::Buffer*, kItemCount> objectBuffers = {};
        for (int i = 0; i < kItemCount; ++i)
        {
            const ObjectUniforms obj = BuildObjectUniforms(kSceneItems[i], sceneUniforms);
            objectBuffers[i] = MakeSharedBuffer(device, &obj, sizeof(obj));
        }

        // 地面物体 uniform（单独处理，尺寸不同）
        SceneItem groundItem = {{0.0f, 0.0f, 0.0f}, {0.42f, 0.44f, 0.48f}, 0.85f, 0.0f, nullptr};
        ObjectUniforms groundObj = BuildObjectUniforms(groundItem, sceneUniforms);
        MTL::Buffer* groundBuffer = MakeSharedBuffer(device, &groundObj, sizeof(groundObj));

        MTL::CommandBuffer* commandBuffer = commandQueue->commandBuffer();
        if (commandBuffer == nullptr)
        {
            std::cerr << "无法创建 MTLCommandBuffer。\n";
            pool->drain();
            return 1;
        }

        // ── Pass 0: Compute 粒子更新 ──
        {
            // 每帧重建 compute 参数缓冲（内容可能因 frameIndex 变化而更新）
            IRBufferView particleBV = {
                .buffer = particleBuffer, .bufferOffset = 0,
                .bufferSize = static_cast<std::uint64_t>(particleBuffer->length()),
            };
            IRBufferView indirectArgsBV = {
                .buffer = indirectArgsBuffer, .bufferOffset = 0,
                .bufferSize = static_cast<std::uint64_t>(indirectArgsBuffer->length()),
            };
            IRBufferView simParamsBV = {
                .buffer = simParamsBuffer, .bufferOffset = 0,
                .bufferSize = static_cast<std::uint64_t>(simParamsBuffer->length()),
            };

            IRDescriptorTableEntry computeEntries[3] = {};
            IRDescriptorTableSetBufferView(&computeEntries[0], &particleBV);
            IRDescriptorTableSetBufferView(&computeEntries[1], &indirectArgsBV);
            IRDescriptorTableSetBufferView(&computeEntries[2], &simParamsBV);
            MTL::Buffer* computeArgBuffer = MakeSharedBuffer(device, computeEntries, sizeof(computeEntries));

            MTL::ComputeCommandEncoder* computeEncoder = commandBuffer->computeCommandEncoder();
            computeEncoder->setComputePipelineState(computePipeline);
            computeEncoder->setBuffer(computeArgBuffer, 0, kIRArgumentBufferBindPoint);
            computeEncoder->useResource(particleBuffer, MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
            computeEncoder->useResource(indirectArgsBuffer, MTL::ResourceUsageRead | MTL::ResourceUsageWrite);
            computeEncoder->dispatchThreadgroups(
                MTL::Size::Make((kParticleCount + kThreadgroupSize - 1) / kThreadgroupSize, 1, 1),
                MTL::Size::Make(kThreadgroupSize, 1, 1));
            computeEncoder->endEncoding();
        }

        // ── Pass 1: Shadow Map ──
        {
            MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
            pass->depthAttachment()->setTexture(shadowTexture);
            pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
            pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
            pass->depthAttachment()->setClearDepth(1.0);

            MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
            encoder->setRenderPipelineState(shadowPipeline);
            encoder->setDepthStencilState(shadowDepthState);
            encoder->setCullMode(MTL::CullModeBack);
            encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);

            // 球体阴影
            for (int i = 0; i < kItemCount; ++i)
            {
                encoder->setVertexBuffer(sphereBuffer, 0, 0);
                encoder->setVertexBuffer(objectBuffers[i], 0, 1);
                encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(kSphereIndexCount));
            }
            // 地面阴影
            encoder->setVertexBuffer(planeBuffer, 0, 0);
            encoder->setVertexBuffer(groundBuffer, 0, 1);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(kPlaneIndexCount));
            encoder->endEncoding();
        }

        // ── Pass 2: HDR 场景 (PBR 球体 + 地面) ──
        {
            MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
            pass->colorAttachments()->object(0)->setTexture(hdrTexture);
            pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
            pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
            pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.0, 0.0, 0.0, 1.0));
            pass->depthAttachment()->setTexture(hdrDepth);
            pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
            pass->depthAttachment()->setStoreAction(MTL::StoreActionDontCare);
            pass->depthAttachment()->setClearDepth(1.0);

            MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
            encoder->setRenderPipelineState(scenePipeline);
            encoder->setDepthStencilState(sceneDepthState);
            encoder->setCullMode(MTL::CullModeBack);
            encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
            encoder->setFragmentTexture(shadowTexture, 0);

            // PBR 球体
            for (int i = 0; i < kItemCount; ++i)
            {
                encoder->setVertexBuffer(sphereBuffer, 0, 0);
                encoder->setVertexBuffer(objectBuffers[i], 0, 1);
                encoder->setFragmentBuffer(objectBuffers[i], 0, 0);
                encoder->setFragmentBuffer(sceneBuffer, 0, 1);
                encoder->setFragmentSamplerState(linearSampler, 0);
                encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(kSphereIndexCount));
            }
            // 地面
            encoder->setVertexBuffer(planeBuffer, 0, 0);
            encoder->setVertexBuffer(groundBuffer, 0, 1);
            encoder->setFragmentBuffer(groundBuffer, 0, 0);
            encoder->setFragmentBuffer(sceneBuffer, 0, 1);
            encoder->setFragmentSamplerState(linearSampler, 0);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(kPlaneIndexCount));

            // 天空盒（关闭深度写入，最后绘制）
            encoder->setRenderPipelineState(skyPipeline);
            encoder->setDepthStencilState(nullptr);
            encoder->setCullMode(MTL::CullModeFront); // 天空球从内部看
            encoder->setVertexBuffer(sphereBuffer, 0, 0);
            // 天空球：巨大的球体包裹整个场景
            SceneItem skyItem = {{0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, 0.0f, 0.0f, nullptr};
            ObjectUniforms skyObj = BuildObjectUniforms(skyItem, sceneUniforms);
            // 覆盖 model matrix：放大到包围场景
            float skyScaleMat[16] = {};
            skyScaleMat[0] = 15.0f;
            skyScaleMat[5] = 15.0f;
            skyScaleMat[10] = 15.0f;
            skyScaleMat[15] = 1.0f;
            std::memcpy(skyObj.modelMatrix, skyScaleMat, sizeof(skyScaleMat));

            Mat4 viewProj = {};
            std::memcpy(viewProj.m, sceneUniforms.viewProj, sizeof(viewProj.m));
            Mat4 skyMvp = Mat4Mul(viewProj, {skyScaleMat[0], skyScaleMat[1], skyScaleMat[2], skyScaleMat[3],
                                            skyScaleMat[4], skyScaleMat[5], skyScaleMat[6], skyScaleMat[7],
                                            skyScaleMat[8], skyScaleMat[9], skyScaleMat[10], skyScaleMat[11],
                                            skyScaleMat[12], skyScaleMat[13], skyScaleMat[14], skyScaleMat[15]});
            std::memcpy(skyObj.modelViewProj, skyMvp.m, sizeof(skyMvp.m));
            MTL::Buffer* skyBuffer = MakeSharedBuffer(device, &skyObj, sizeof(skyObj));

            encoder->setVertexBuffer(skyBuffer, 0, 1);
            encoder->setFragmentBuffer(skyBuffer, 0, 0);
            encoder->setFragmentBuffer(sceneBuffer, 0, 1);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(kSphereIndexCount));

            encoder->endEncoding();
        }

        // ── Pass 3: 后处理 Bloom ──
        auto encodeFullscreen = [&](MTL::Texture* target, MTL::RenderPipelineState* pipeline,
                                    MTL::Texture* input0, MTL::Texture* input1,
                                    bool bindScene)
        {
            MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
            pass->colorAttachments()->object(0)->setTexture(target);
            pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
            pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
            pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.0, 0.0, 0.0, 1.0));
            MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
            encoder->setRenderPipelineState(pipeline);
            if (bindScene)
            {
                encoder->setFragmentBuffer(sceneBuffer, 0, 0);
            }
            if (input0 != nullptr)
            {
                encoder->setFragmentTexture(input0, 0);
            }
            if (input1 != nullptr)
            {
                encoder->setFragmentTexture(input1, 1);
            }
            encoder->setFragmentSamplerState(linearSampler, 0);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
            encoder->endEncoding();
        };

        encodeFullscreen(brightTexture, brightPipeline, hdrTexture, nullptr, false);
        encodeFullscreen(blurPingTexture, blurHPipeline, brightTexture, nullptr, false);
        encodeFullscreen(blurPongTexture, blurVPipeline, blurPingTexture, nullptr, false);
        encodeFullscreen(compositeTexture, compositePipeline, hdrTexture, blurPongTexture, true);

        // ── Pass 4: 粒子 (加法混合覆盖) ──
        {
            MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
            pass->colorAttachments()->object(0)->setTexture(compositeTexture);
            pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionLoad);
            pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

            MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
            encoder->setRenderPipelineState(particlePipeline);
            encoder->setCullMode(MTL::CullModeNone);
            encoder->setVertexBuffer(particleBuffer, 0, 0);
            encoder->setVertexBuffer(particleRenderParamsBuffer, 0, 1);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, indirectArgsBuffer, 0);
            encoder->endEncoding();
        }

        // ── Pass 5: HUD 覆盖层 ──
        {
            MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
            pass->colorAttachments()->object(0)->setTexture(compositeTexture);
            pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionLoad);
            pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);

            MTL::RenderCommandEncoder* encoder = commandBuffer->renderCommandEncoder(pass);
            encoder->setRenderPipelineState(hudPipeline);
            encoder->setVertexBuffer(hudBuffer, 0, 0);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0),
                                    NS::UInteger(hudPositions.size() / 2));
            encoder->endEncoding();
        }

        commandBuffer->commit();
        commandBuffer->waitUntilCompleted();

        // 统计粒子间接绘制参数
        const DrawArgs* currentArgs = static_cast<const DrawArgs*>(indirectArgsBuffer->contents());
        minInstanceCount = std::min(minInstanceCount, currentArgs->instanceCount);
        maxInstanceCount = std::max(maxInstanceCount, currentArgs->instanceCount);
        finalInstanceCount = currentArgs->instanceCount;
    }

    // ── 性能统计 ──
    const auto end = std::chrono::steady_clock::now();
    const double sampleSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin).count();
    const double avgFrameMs = sampleSeconds * 1000.0 / static_cast<double>(kSampleFrames);
    const double avgFps = static_cast<double>(kSampleFrames) / sampleSeconds;

    // ── 读回最终帧缓冲 ──
    std::vector<Pixel> pixels(kWidth * kHeight);
    compositeTexture->getBytes(
        pixels.data(),
        static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
        MTL::Region::Make2D(0, 0, kWidth, kHeight),
        0);

    // ── 写出 PPM ──
    std::filesystem::create_directories("out");
    if (!WritePpm(std::filesystem::path("out") / "complex_showcase.ppm", pixels))
    {
        std::cerr << "无法写出 D8 渲染结果。\n";
        pool->drain();
        return 1;
    }

    // ── 写出性能 JSON ──
    const PerfStats stats = {
        .avgFps = avgFps,
        .avgFrameMs = avgFrameMs,
        .sampleSeconds = sampleSeconds,
        .minInstanceCount = minInstanceCount,
        .maxInstanceCount = maxInstanceCount,
        .finalInstanceCount = finalInstanceCount,
    };
    if (!WritePerfJson(std::filesystem::path("out") / "complex_showcase_perf.json", deviceName, stats))
    {
        std::cerr << "无法写出 D8 性能记录。\n";
        pool->drain();
        return 1;
    }

    std::cout << "\nD8 Complex Showcase 完成: out/complex_showcase.ppm\n";
    std::cout << std::fixed << std::setprecision(2)
              << "平均 FPS: " << avgFps
              << ", 平均帧时间: " << avgFrameMs << " ms"
              << ", 粒子 instanceCount 范围: [" << minInstanceCount << ", " << maxInstanceCount << "]\n";
    std::cout << "渲染通道: Shadow → Scene(PBR+Skybox) → Bloom(3pass) → Composite → Particles → HUD\n";
    std::cout << "PBR 模型: Cook-Torrance GGX, " << kItemCount << " 个球体 "
              << "(Gold/Chrome/Copper/Plastic/Ceramic/Blue Metal/Green Metal)\n";
    std::cout << "摄像机: 自由轨道 (自动旋转 + 高度振荡)\n";

    pool->drain();
    return 0;
}
