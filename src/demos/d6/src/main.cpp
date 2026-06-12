#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define IR_RUNTIME_METALCPP
#define IR_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t kWidth = 768;
constexpr std::uint32_t kHeight = 768;
constexpr std::uint32_t kShadowSize = 1024;
constexpr std::uint32_t kBloomWidth = 384;
constexpr std::uint32_t kBloomHeight = 384;
constexpr const char* kPathAShadowVertexMetallib = "build/d6_shadow_vertex.metallib";
constexpr const char* kPathASceneVertexMetallib = "build/d6_scene_vertex.metallib";
constexpr const char* kPathASceneFragmentMetallib = "build/d6_scene_fragment.metallib";

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

struct alignas(16) SceneUniforms
{
    float viewProj[16];
    float lightViewProj[16];
    float lightDir[3];
    float exposure;
    float cameraPos[3];
    float bloomStrength;
};

struct alignas(16) ObjectUniforms
{
    float modelMatrix[16];
    float modelViewProj[16];
    float lightMvp[16];
    float baseColor[4];
    float emissiveColor[4];
};

static_assert(sizeof(SceneUniforms) == 160, "SceneUniforms 大小必须为 160 字节");
static_assert(sizeof(ObjectUniforms) == 224, "ObjectUniforms 大小必须为 224 字节");

enum class GeometryType
{
    Plane,
    Cube,
};

struct DrawItem
{
    GeometryType geometry;
    Float3 translation;
    Float3 rotation;
    Float3 scale;
    Float3 baseColor;
    Float3 emissiveColor;
};

enum class SceneShaderPath
{
    PathA,
    LegacyMsl,
};

constexpr Vertex kCubeVertices[] = {
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  0.0f,  1.0f}},

    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  0.0f, -1.0f}},

    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f, -0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f, -0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f,  0.5f,  0.5f}, {-1.0f,  0.0f,  0.0f}},
    {{-0.5f,  0.5f, -0.5f}, {-1.0f,  0.0f,  0.0f}},

    {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 1.0f,  0.0f,  0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 1.0f,  0.0f,  0.0f}},

    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{-0.5f,  0.5f,  0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},
    {{-0.5f,  0.5f, -0.5f}, { 0.0f,  1.0f,  0.0f}},

    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{-0.5f, -0.5f, -0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{ 0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
    {{-0.5f, -0.5f,  0.5f}, { 0.0f, -1.0f,  0.0f}},
};

constexpr Vertex kPlaneVertices[] = {
    {{-5.5f, 0.0f, -5.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 5.5f, 0.0f, -5.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 5.5f, 0.0f,  5.5f}, {0.0f, 1.0f, 0.0f}},
    {{-5.5f, 0.0f, -5.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 5.5f, 0.0f,  5.5f}, {0.0f, 1.0f, 0.0f}},
    {{-5.5f, 0.0f,  5.5f}, {0.0f, 1.0f, 0.0f}},
};

constexpr std::array<DrawItem, 5> kSceneItems = {{
    {GeometryType::Plane, {0.0f, -1.15f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.38f, 0.40f, 0.46f}, {0.0f, 0.0f, 0.0f}},
    {GeometryType::Cube,  {-1.55f, -0.35f, 0.95f}, {0.0f, 0.38f, 0.0f}, {0.95f, 1.60f, 0.95f}, {0.82f, 0.42f, 0.20f}, {0.0f, 0.0f, 0.0f}},
    {GeometryType::Cube,  { 1.10f, -0.45f, 0.10f}, {0.0f, -0.32f, 0.0f}, {1.10f, 1.10f, 1.10f}, {0.24f, 0.62f, 0.90f}, {0.0f, 0.0f, 0.0f}},
    {GeometryType::Cube,  { 0.10f, -0.25f, 1.85f}, {0.0f, 0.65f, 0.0f}, {0.72f, 1.85f, 0.72f}, {0.28f, 0.86f, 0.48f}, {0.0f, 0.0f, 0.0f}},
    {GeometryType::Cube,  { 0.45f, 1.10f, -0.35f}, {0.0f, 0.22f, 0.0f}, {0.48f, 0.48f, 0.48f}, {0.98f, 0.82f, 0.22f}, {6.2f, 5.2f, 2.6f}},
}};

const char* kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct SceneUniforms
{
    float4x4 viewProj;
    float4x4 lightViewProj;
    packed_float3 lightDir;
    float exposure;
    packed_float3 cameraPos;
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

vertex ShadowOut shadowVertex(VertexIn in [[stage_in]],
                              constant ObjectUniforms& object [[buffer(1)]])
{
    ShadowOut out;
    out.position = object.lightMvp * float4(in.position, 1.0);
    return out;
}

vertex SceneOut sceneVertex(VertexIn in [[stage_in]],
                            constant ObjectUniforms& object [[buffer(1)]])
{
    SceneOut out;
    float4 worldPos4 = object.modelMatrix * float4(in.position, 1.0);
    float3x3 normalMatrix = float3x3(
        object.modelMatrix[0].xyz,
        object.modelMatrix[1].xyz,
        object.modelMatrix[2].xyz);
    out.position = object.modelViewProj * float4(in.position, 1.0);
    out.worldPos = worldPos4.xyz;
    out.worldNormal = normalize(normalMatrix * in.normal);
    out.shadowPos = object.lightMvp * float4(in.position, 1.0);
    return out;
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

    float3 shadowCoord = in.shadowPos.xyz / in.shadowPos.w;
    float2 shadowUv = shadowCoord.xy * 0.5 + 0.5;
    shadowUv.y = 1.0 - shadowUv.y;
    float shadow = 1.0;
    if (in.shadowPos.w > 0.0 &&
        shadowUv.x >= 0.0 && shadowUv.x <= 1.0 &&
        shadowUv.y >= 0.0 && shadowUv.y <= 1.0 &&
        shadowCoord.z >= 0.0 && shadowCoord.z <= 1.0)
    {
        shadow = shadowTexture.sample_compare(shadowSampler, shadowUv, shadowCoord.z - 0.0018);
    }

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 48.0);
    float3 ambient = object.baseColor.rgb * 0.16;
    float3 diffuse = object.baseColor.rgb * diff * shadow;
    float3 specular = float3(0.30, 0.32, 0.36) * spec * shadow;
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * 0.12;
    float3 color = ambient + diffuse + specular + object.baseColor.rgb * rim + object.emissiveColor.rgb;
    return float4(color, 1.0);
}

vertex QuadOut quadVertex(uint vertexId [[vertex_id]])
{
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    QuadOut out;
    out.position = float4(positions[vertexId], 0.0, 1.0);
    out.uv = positions[vertexId] * 0.5 + 0.5;
    return out;
}

fragment float4 brightExtractFragment(QuadOut in [[stage_in]],
                                      texture2d<float> hdrTexture [[texture(0)]],
                                      sampler linearSampler [[sampler(0)]])
{
    float3 color = hdrTexture.sample(linearSampler, in.uv).rgb;
    float luminance = dot(color, float3(0.2126, 0.7152, 0.0722));
    float intensity = smoothstep(1.1, 2.7, luminance);
    return float4(color * intensity, 1.0);
}

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

fragment float4 compositeFragment(QuadOut in [[stage_in]],
                                  constant SceneUniforms& scene [[buffer(0)]],
                                  texture2d<float> hdrTexture [[texture(0)]],
                                  texture2d<float> bloomTexture [[texture(1)]],
                                  sampler linearSampler [[sampler(0)]])
{
    float3 hdr = hdrTexture.sample(linearSampler, in.uv).rgb;
    float3 bloom = bloomTexture.sample(linearSampler, in.uv).rgb * scene.bloomStrength;
    float3 color = hdr + bloom;
    float3 mapped = 1.0 - exp(-color * scene.exposure);
    mapped = pow(mapped, float3(1.0 / 2.2));
    return float4(mapped, 1.0);
}
)";

Float3 Add(const Float3& a, const Float3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
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

std::string ErrorToString(NS::Error* error)
{
    if (error == nullptr)
    {
        return "未知错误";
    }

    NS::String* description = error->localizedDescription();
    return description != nullptr ? description->utf8String() : "未知错误";
}

bool WritePpm(const std::filesystem::path& output_path, const std::vector<Pixel>& pixels)
{
    std::ofstream output(output_path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P6\n" << kWidth << " " << kHeight << "\n255\n";
    for (std::uint32_t y = 0; y < kHeight; ++y)
    {
        const std::uint32_t src_y = kHeight - 1 - y;
        for (std::uint32_t x = 0; x < kWidth; ++x)
        {
            const Pixel& pixel = pixels[src_y * kWidth + x];
            output.put(static_cast<char>(pixel.r));
            output.put(static_cast<char>(pixel.g));
            output.put(static_cast<char>(pixel.b));
        }
    }
    return true;
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
    result.m[15] = 1.0f;
    return result;
}

Mat4 Mat4RotateX(float angle)
{
    Mat4 result = Mat4Identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;
    return result;
}

Mat4 Mat4RotateY(float angle)
{
    Mat4 result = Mat4Identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[0] = c;
    result.m[2] = -s;
    result.m[8] = s;
    result.m[10] = c;
    return result;
}

Mat4 Mat4RotateZ(float angle)
{
    Mat4 result = Mat4Identity();
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    result.m[0] = c;
    result.m[1] = s;
    result.m[4] = -s;
    result.m[5] = c;
    return result;
}

Mat4 Mat4Perspective(float fov_y, float aspect, float near_z, float far_z)
{
    const float tan_half = std::tan(fov_y * 0.5f);
    Mat4 result = {};
    result.m[0] = 1.0f / (aspect * tan_half);
    result.m[5] = 1.0f / tan_half;
    result.m[10] = far_z / (near_z - far_z);
    result.m[11] = -1.0f;
    result.m[14] = (near_z * far_z) / (near_z - far_z);
    return result;
}

Mat4 Mat4Orthographic(float left, float right, float bottom, float top, float near_z, float far_z)
{
    Mat4 result = Mat4Identity();
    result.m[0] = 2.0f / (right - left);
    result.m[5] = 2.0f / (top - bottom);
    result.m[10] = 1.0f / (near_z - far_z);
    result.m[12] = (left + right) / (left - right);
    result.m[13] = (top + bottom) / (bottom - top);
    result.m[14] = near_z / (near_z - far_z);
    return result;
}

Mat4 Mat4LookAt(Float3 eye, Float3 target, Float3 up)
{
    const Float3 forward = Normalize({target.x - eye.x, target.y - eye.y, target.z - eye.z});
    const Float3 side = Normalize(Cross(forward, up));
    const Float3 true_up = Cross(side, forward);

    Mat4 result = Mat4Identity();
    result.m[0] = side.x;
    result.m[4] = side.y;
    result.m[8] = side.z;
    result.m[12] = -Dot(side, eye);
    result.m[1] = true_up.x;
    result.m[5] = true_up.y;
    result.m[9] = true_up.z;
    result.m[13] = -Dot(true_up, eye);
    result.m[2] = -forward.x;
    result.m[6] = -forward.y;
    result.m[10] = -forward.z;
    result.m[14] = Dot(forward, eye);
    return result;
}

Mat4 BuildModelMatrix(const DrawItem& item)
{
    return Mat4Mul(
        Mat4Translate(item.translation.x, item.translation.y, item.translation.z),
        Mat4Mul(
            Mat4RotateY(item.rotation.y),
            Mat4Mul(
                Mat4RotateX(item.rotation.x),
                Mat4Mul(Mat4RotateZ(item.rotation.z), Mat4Scale(item.scale.x, item.scale.y, item.scale.z)))));
}

SceneUniforms BuildSceneUniforms()
{
    const Float3 camera = {4.6f, 3.2f, 7.1f};
    const Float3 target = {0.0f, -0.2f, 0.7f};
    const Mat4 view = Mat4LookAt(camera, target, {0.0f, 1.0f, 0.0f});
    const Mat4 proj = Mat4Perspective(3.14159f * 0.19f, static_cast<float>(kWidth) / static_cast<float>(kHeight), 0.1f, 28.0f);

    const Float3 light_dir = Normalize({-0.45f, -0.84f, -0.30f});
    const Float3 light_eye = Add(target, Scale(light_dir, -8.8f));
    const Mat4 light_view = Mat4LookAt(light_eye, target, {0.0f, 1.0f, 0.0f});
    const Mat4 light_proj = Mat4Orthographic(-7.0f, 7.0f, -7.0f, 7.0f, 0.1f, 22.0f);

    SceneUniforms uniforms = {};
    const Mat4 view_proj = Mat4Mul(proj, view);
    const Mat4 light_view_proj = Mat4Mul(light_proj, light_view);
    std::memcpy(uniforms.viewProj, view_proj.m, sizeof(view_proj.m));
    std::memcpy(uniforms.lightViewProj, light_view_proj.m, sizeof(light_view_proj.m));
    uniforms.lightDir[0] = light_dir.x;
    uniforms.lightDir[1] = light_dir.y;
    uniforms.lightDir[2] = light_dir.z;
    uniforms.exposure = 1.05f;
    uniforms.cameraPos[0] = camera.x;
    uniforms.cameraPos[1] = camera.y;
    uniforms.cameraPos[2] = camera.z;
    uniforms.bloomStrength = 0.58f;
    return uniforms;
}

ObjectUniforms BuildObjectUniforms(const DrawItem& item, const SceneUniforms& scene)
{
    ObjectUniforms uniforms = {};
    const Mat4 model = BuildModelMatrix(item);

    Mat4 view_proj = {};
    Mat4 light_view_proj = {};
    std::memcpy(view_proj.m, scene.viewProj, sizeof(view_proj.m));
    std::memcpy(light_view_proj.m, scene.lightViewProj, sizeof(light_view_proj.m));

    const Mat4 mvp = Mat4Mul(view_proj, model);
    const Mat4 light_mvp = Mat4Mul(light_view_proj, model);

    std::memcpy(uniforms.modelMatrix, model.m, sizeof(model.m));
    std::memcpy(uniforms.modelViewProj, mvp.m, sizeof(mvp.m));
    std::memcpy(uniforms.lightMvp, light_mvp.m, sizeof(light_mvp.m));
    uniforms.baseColor[0] = item.baseColor.x;
    uniforms.baseColor[1] = item.baseColor.y;
    uniforms.baseColor[2] = item.baseColor.z;
    uniforms.baseColor[3] = 1.0f;
    uniforms.emissiveColor[0] = item.emissiveColor.x;
    uniforms.emissiveColor[1] = item.emissiveColor.y;
    uniforms.emissiveColor[2] = item.emissiveColor.z;
    uniforms.emissiveColor[3] = 1.0f;
    return uniforms;
}

MTL::Buffer* MakeSharedBuffer(MTL::Device* device, const void* bytes, std::size_t size)
{
    return device->newBuffer(bytes, static_cast<NS::UInteger>(size), MTL::ResourceStorageModeShared);
}

MTL::Texture* MakeColorTexture(MTL::Device* device,
                               MTL::PixelFormat pixel_format,
                               std::uint32_t width,
                               std::uint32_t height,
                               MTL::TextureUsage usage,
                               MTL::StorageMode storage_mode)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(pixel_format, width, height, false);
    descriptor->setUsage(usage);
    descriptor->setStorageMode(storage_mode);
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
    NS::String* library_path = NS::String::string(path, NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(library_path, &error);
    if (library == nullptr)
    {
        std::cerr << "无法加载 metallib " << path << ": " << ErrorToString(error) << "\n";
    }
    return library;
}

MTL::VertexDescriptor* CreateLegacyVertexDescriptor()
{
    MTL::VertexDescriptor* vertex_descriptor = MTL::VertexDescriptor::alloc()->init();
    vertex_descriptor->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    vertex_descriptor->attributes()->object(0)->setOffset(0);
    vertex_descriptor->attributes()->object(0)->setBufferIndex(0);
    vertex_descriptor->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
    vertex_descriptor->attributes()->object(1)->setOffset(sizeof(float) * 3);
    vertex_descriptor->attributes()->object(1)->setBufferIndex(0);
    vertex_descriptor->layouts()->object(0)->setStride(sizeof(Vertex));
    return vertex_descriptor;
}

MTL::VertexDescriptor* CreatePathAVertexDescriptor()
{
    MTL::VertexDescriptor* vertex_descriptor = MTL::VertexDescriptor::alloc()->init();
    vertex_descriptor->attributes()->object(kIRStageInAttributeStartIndex + 0)->setFormat(MTL::VertexFormatFloat3);
    vertex_descriptor->attributes()->object(kIRStageInAttributeStartIndex + 0)->setOffset(0);
    vertex_descriptor->attributes()->object(kIRStageInAttributeStartIndex + 0)->setBufferIndex(0);
    vertex_descriptor->attributes()->object(kIRStageInAttributeStartIndex + 1)->setFormat(MTL::VertexFormatFloat3);
    vertex_descriptor->attributes()->object(kIRStageInAttributeStartIndex + 1)->setOffset(sizeof(float) * 3);
    vertex_descriptor->attributes()->object(kIRStageInAttributeStartIndex + 1)->setBufferIndex(0);
    vertex_descriptor->layouts()->object(0)->setStride(sizeof(Vertex));
    return vertex_descriptor;
}

IRBufferView MakeConstantBufferView(MTL::Buffer* buffer)
{
    return {
        .buffer = buffer,
        .bufferOffset = 0,
        .bufferSize = static_cast<std::uint64_t>(buffer->length()),
        .textureBufferView = nullptr,
        .textureViewOffsetInElements = 0,
        .typedBuffer = false,
    };
}

MTL::Buffer* MakeArgumentBuffer(MTL::Device* device, const IRDescriptorTableEntry* entries, std::size_t count)
{
    return device->newBuffer(entries,
                             static_cast<NS::UInteger>(sizeof(IRDescriptorTableEntry) * count),
                             MTL::ResourceStorageModeShared);
}

MTL::RenderPipelineState* BuildLegacyShadowPipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertex_fn = library->newFunction(MTLSTR("shadowVertex"));
    MTL::VertexDescriptor* vertex_descriptor = CreateLegacyVertexDescriptor();

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(vertex_fn);
    descriptor->setVertexDescriptor(vertex_descriptor);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 Shadow 管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildLegacyScenePipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertex_fn = library->newFunction(MTLSTR("sceneVertex"));
    MTL::Function* fragment_fn = library->newFunction(MTLSTR("sceneFragment"));
    MTL::VertexDescriptor* vertex_descriptor = CreateLegacyVertexDescriptor();

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(vertex_fn);
    descriptor->setFragmentFunction(fragment_fn);
    descriptor->setVertexDescriptor(vertex_descriptor);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 HDR 场景管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildPathAShadowPipeline(MTL::Device* device, MTL::Library* vertex_library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertex_fn = vertex_library->newFunction(MTLSTR("shadowVertex"));
    if (vertex_fn == nullptr)
    {
        std::cerr << "无法从 Path A shadow metallib 中取出 shadowVertex。\n";
        return nullptr;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(vertex_fn);
    descriptor->setVertexDescriptor(CreatePathAVertexDescriptor());
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 Path A Shadow 管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildPathAScenePipeline(MTL::Device* device,
                                                  MTL::Library* vertex_library,
                                                  MTL::Library* fragment_library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertex_fn = vertex_library->newFunction(MTLSTR("sceneVertex"));
    MTL::Function* fragment_fn = fragment_library->newFunction(MTLSTR("sceneFragment"));
    if (vertex_fn == nullptr || fragment_fn == nullptr)
    {
        std::cerr << "无法从 Path A scene metallib 中取出入口函数。\n";
        return nullptr;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(vertex_fn);
    descriptor->setFragmentFunction(fragment_fn);
    descriptor->setVertexDescriptor(CreatePathAVertexDescriptor());
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatRGBA16Float);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineReflection* reflection = nullptr;
    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(
        descriptor,
        MTL::PipelineOptionArgumentInfo,
        &reflection,
        &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 Path A HDR 场景管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildFullscreenPipeline(MTL::Device* device,
                                                  MTL::Library* library,
                                                  const char* fragment_name,
                                                  MTL::PixelFormat color_format)
{
    NS::Error* error = nullptr;
    MTL::Function* vertex_fn = library->newFunction(MTLSTR("quadVertex"));
    NS::String* fragment_name_ns = NS::String::string(fragment_name, NS::UTF8StringEncoding);
    MTL::Function* fragment_fn = library->newFunction(fragment_name_ns);

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(vertex_fn);
    descriptor->setFragmentFunction(fragment_fn);
    descriptor->colorAttachments()->object(0)->setPixelFormat(color_format);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建后处理管线 " << fragment_name << ": " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::DepthStencilState* BuildDepthState(MTL::Device* device, MTL::CompareFunction compare, bool write_enabled)
{
    MTL::DepthStencilDescriptor* descriptor = MTL::DepthStencilDescriptor::alloc()->init();
    descriptor->setDepthCompareFunction(compare);
    descriptor->setDepthWriteEnabled(write_enabled);
    return device->newDepthStencilState(descriptor);
}

void EncodeDrawItems(MTL::RenderCommandEncoder* encoder,
                     MTL::Buffer* cube_buffer,
                     MTL::Buffer* plane_buffer,
                     const std::array<MTL::Buffer*, kSceneItems.size()>& object_buffers,
                     bool is_scene_pass,
                     MTL::Buffer* scene_buffer)
{
    for (std::size_t index = 0; index < kSceneItems.size(); ++index)
    {
        MTL::Buffer* vertex_buffer = kSceneItems[index].geometry == GeometryType::Plane ? plane_buffer : cube_buffer;
        const NS::UInteger vertex_count = kSceneItems[index].geometry == GeometryType::Plane ? 6 : 36;
        encoder->setVertexBuffer(vertex_buffer, 0, 0);
        encoder->setVertexBuffer(object_buffers[index], 0, 1);
        if (is_scene_pass)
        {
            encoder->setFragmentBuffer(object_buffers[index], 0, 0);
            encoder->setFragmentBuffer(scene_buffer, 0, 1);
        }
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), vertex_count);
    }
}

void EncodeShadowDrawItemsPathA(MTL::RenderCommandEncoder* encoder,
                                MTL::Buffer* cube_buffer,
                                MTL::Buffer* plane_buffer,
                                const std::array<MTL::Buffer*, kSceneItems.size()>& vertex_argument_buffers)
{
    for (std::size_t index = 0; index < kSceneItems.size(); ++index)
    {
        MTL::Buffer* vertex_buffer = kSceneItems[index].geometry == GeometryType::Plane ? plane_buffer : cube_buffer;
        const NS::UInteger vertex_count = kSceneItems[index].geometry == GeometryType::Plane ? 6 : 36;
        encoder->setVertexBuffer(vertex_buffer, 0, 0);
        encoder->setVertexBuffer(vertex_argument_buffers[index], 0, kIRArgumentBufferBindPoint);
        IRRuntimeDrawPrimitives(encoder,
                                MTL::PrimitiveTypeTriangle,
                                static_cast<std::uint64_t>(0),
                                static_cast<std::uint64_t>(vertex_count));
    }
}

void EncodeSceneDrawItemsPathA(MTL::RenderCommandEncoder* encoder,
                               MTL::Buffer* cube_buffer,
                               MTL::Buffer* plane_buffer,
                               const std::array<MTL::Buffer*, kSceneItems.size()>& vertex_argument_buffers,
                               const std::array<MTL::Buffer*, kSceneItems.size()>& fragment_argument_buffers)
{
    for (std::size_t index = 0; index < kSceneItems.size(); ++index)
    {
        MTL::Buffer* vertex_buffer = kSceneItems[index].geometry == GeometryType::Plane ? plane_buffer : cube_buffer;
        const NS::UInteger vertex_count = kSceneItems[index].geometry == GeometryType::Plane ? 6 : 36;
        encoder->setVertexBuffer(vertex_buffer, 0, 0);
        encoder->setVertexBuffer(vertex_argument_buffers[index], 0, kIRArgumentBufferBindPoint);
        encoder->setFragmentBuffer(fragment_argument_buffers[index], 0, kIRArgumentBufferBindPoint);
        IRRuntimeDrawPrimitives(encoder,
                                MTL::PrimitiveTypeTriangle,
                                static_cast<std::uint64_t>(0),
                                static_cast<std::uint64_t>(vertex_count));
    }
}

SceneShaderPath ParseSceneShaderPath(int argc, char** argv)
{
    for (int index = 1; index < argc; ++index)
    {
        if (std::strcmp(argv[index], "--legacy-msl") == 0)
        {
            return SceneShaderPath::LegacyMsl;
        }
    }
    return SceneShaderPath::PathA;
}

} // namespace

int main(int argc, char** argv)
{
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    const SceneShaderPath scene_shader_path = ParseSceneShaderPath(argc, argv);

    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (device == nullptr)
    {
        std::cerr << "无法创建 MTLDevice。\n";
        pool->drain();
        return 1;
    }
    std::cout << "设备: " << device->name()->utf8String() << "\n";

    MTL::CommandQueue* command_queue = device->newCommandQueue();
    if (command_queue == nullptr)
    {
        std::cerr << "无法创建 MTLCommandQueue。\n";
        pool->drain();
        return 1;
    }

    NS::Error* error = nullptr;
    NS::String* source = NS::String::string(kShaderSource, NS::UTF8StringEncoding);
    MTL::Library* msl_library = device->newLibrary(source, nullptr, &error);
    if (msl_library == nullptr)
    {
        std::cerr << "MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "MSL 后处理着色器编译通过\n";

    MTL::Library* patha_shadow_vertex_library = nullptr;
    MTL::Library* patha_scene_vertex_library = nullptr;
    MTL::Library* patha_scene_fragment_library = nullptr;
    if (scene_shader_path == SceneShaderPath::PathA)
    {
        patha_shadow_vertex_library = LoadMetallib(device, kPathAShadowVertexMetallib);
        patha_scene_vertex_library = LoadMetallib(device, kPathASceneVertexMetallib);
        patha_scene_fragment_library = LoadMetallib(device, kPathASceneFragmentMetallib);
        if (patha_shadow_vertex_library == nullptr || patha_scene_vertex_library == nullptr || patha_scene_fragment_library == nullptr)
        {
            pool->drain();
            return 1;
        }
        std::cout << "已加载 Path A metallib: shadow/scene pass\n";
    }

    const SceneUniforms scene_uniforms = BuildSceneUniforms();
    MTL::Buffer* scene_buffer = MakeSharedBuffer(device, &scene_uniforms, sizeof(scene_uniforms));
    MTL::Buffer* cube_buffer = MakeSharedBuffer(device, kCubeVertices, sizeof(kCubeVertices));
    MTL::Buffer* plane_buffer = MakeSharedBuffer(device, kPlaneVertices, sizeof(kPlaneVertices));

    std::array<MTL::Buffer*, kSceneItems.size()> object_buffers = {};
    for (std::size_t index = 0; index < kSceneItems.size(); ++index)
    {
        const ObjectUniforms object_uniforms = BuildObjectUniforms(kSceneItems[index], scene_uniforms);
        object_buffers[index] = MakeSharedBuffer(device, &object_uniforms, sizeof(object_uniforms));
    }

    if (scene_buffer == nullptr || cube_buffer == nullptr || plane_buffer == nullptr)
    {
        std::cerr << "无法创建 D6 基础缓冲。\n";
        pool->drain();
        return 1;
    }

    MTL::RenderPipelineState* shadow_pipeline = scene_shader_path == SceneShaderPath::PathA
        ? BuildPathAShadowPipeline(device, patha_shadow_vertex_library)
        : BuildLegacyShadowPipeline(device, msl_library);
    MTL::RenderPipelineState* scene_pipeline = scene_shader_path == SceneShaderPath::PathA
        ? BuildPathAScenePipeline(device, patha_scene_vertex_library, patha_scene_fragment_library)
        : BuildLegacyScenePipeline(device, msl_library);
    MTL::RenderPipelineState* bright_pipeline = BuildFullscreenPipeline(device, msl_library, "brightExtractFragment", MTL::PixelFormatRGBA16Float);
    MTL::RenderPipelineState* blur_h_pipeline = BuildFullscreenPipeline(device, msl_library, "blurHorizontalFragment", MTL::PixelFormatRGBA16Float);
    MTL::RenderPipelineState* blur_v_pipeline = BuildFullscreenPipeline(device, msl_library, "blurVerticalFragment", MTL::PixelFormatRGBA16Float);
    MTL::RenderPipelineState* composite_pipeline = BuildFullscreenPipeline(device, msl_library, "compositeFragment", MTL::PixelFormatBGRA8Unorm);
    if (shadow_pipeline == nullptr || scene_pipeline == nullptr || bright_pipeline == nullptr ||
        blur_h_pipeline == nullptr || blur_v_pipeline == nullptr || composite_pipeline == nullptr)
    {
        pool->drain();
        return 1;
    }

    MTL::DepthStencilState* shadow_depth_state = BuildDepthState(device, MTL::CompareFunctionLess, true);
    MTL::DepthStencilState* scene_depth_state = BuildDepthState(device, MTL::CompareFunctionLess, true);

    MTL::SamplerDescriptor* linear_sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    linear_sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
    linear_sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);
    linear_sampler_descriptor->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
    linear_sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    linear_sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    MTL::SamplerState* linear_sampler = device->newSamplerState(linear_sampler_descriptor);

    MTL::SamplerDescriptor* shadow_sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    shadow_sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
    shadow_sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);
    shadow_sampler_descriptor->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
    shadow_sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    shadow_sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    shadow_sampler_descriptor->setCompareFunction(MTL::CompareFunctionLessEqual);
    shadow_sampler_descriptor->setSupportArgumentBuffers(true);
    MTL::SamplerState* shadow_sampler = device->newSamplerState(shadow_sampler_descriptor);

    if (linear_sampler == nullptr || shadow_sampler == nullptr)
    {
        std::cerr << "无法创建 D6 采样器。\n";
        pool->drain();
        return 1;
    }

    MTL::Texture* shadow_texture = MakeDepthTexture(device, kShadowSize, kShadowSize, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    MTL::Texture* hdr_texture = MakeColorTexture(device, MTL::PixelFormatRGBA16Float, kWidth, kHeight, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
    MTL::Texture* hdr_depth = MakeDepthTexture(device, kWidth, kHeight, MTL::TextureUsageRenderTarget);
    MTL::Texture* bright_texture = MakeColorTexture(device, MTL::PixelFormatRGBA16Float, kBloomWidth, kBloomHeight, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
    MTL::Texture* blur_ping_texture = MakeColorTexture(device, MTL::PixelFormatRGBA16Float, kBloomWidth, kBloomHeight, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
    MTL::Texture* blur_pong_texture = MakeColorTexture(device, MTL::PixelFormatRGBA16Float, kBloomWidth, kBloomHeight, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModePrivate);
    MTL::Texture* final_texture = MakeColorTexture(device, MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModeShared);
    if (shadow_texture == nullptr || hdr_texture == nullptr || hdr_depth == nullptr || bright_texture == nullptr ||
        blur_ping_texture == nullptr || blur_pong_texture == nullptr || final_texture == nullptr)
    {
        std::cerr << "无法创建 D6 渲染目标纹理。\n";
        pool->drain();
        return 1;
    }

    std::array<MTL::Buffer*, kSceneItems.size()> patha_vertex_argument_buffers = {};
    std::array<MTL::Buffer*, kSceneItems.size()> patha_fragment_argument_buffers = {};
    if (scene_shader_path == SceneShaderPath::PathA)
    {
        const IRBufferView scene_buffer_view = MakeConstantBufferView(scene_buffer);
        for (std::size_t index = 0; index < kSceneItems.size(); ++index)
        {
            const IRBufferView object_buffer_view = MakeConstantBufferView(object_buffers[index]);

            IRDescriptorTableEntry vertex_arguments[1] = {};
            IRDescriptorTableSetBufferView(&vertex_arguments[0], &object_buffer_view);
            patha_vertex_argument_buffers[index] = MakeArgumentBuffer(device, vertex_arguments, 1);

            IRDescriptorTableEntry fragment_arguments[4] = {};
            IRDescriptorTableSetTexture(&fragment_arguments[0], shadow_texture, 0.0f, 0);
            IRDescriptorTableSetBufferView(&fragment_arguments[1], &scene_buffer_view);
            IRDescriptorTableSetBufferView(&fragment_arguments[2], &object_buffer_view);
            IRDescriptorTableSetSampler(&fragment_arguments[3], shadow_sampler, 0.0f);
            patha_fragment_argument_buffers[index] = MakeArgumentBuffer(device, fragment_arguments, 4);
        }
    }

    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();

    MTL::RenderPassDescriptor* shadow_pass = MTL::RenderPassDescriptor::alloc()->init();
    shadow_pass->depthAttachment()->setTexture(shadow_texture);
    shadow_pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
    shadow_pass->depthAttachment()->setStoreAction(MTL::StoreActionStore);
    shadow_pass->depthAttachment()->setClearDepth(1.0);
    MTL::RenderCommandEncoder* shadow_encoder = command_buffer->renderCommandEncoder(shadow_pass);
    shadow_encoder->setRenderPipelineState(shadow_pipeline);
    shadow_encoder->setDepthStencilState(shadow_depth_state);
    shadow_encoder->setCullMode(MTL::CullModeBack);
    shadow_encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
    if (scene_shader_path == SceneShaderPath::PathA)
    {
        EncodeShadowDrawItemsPathA(shadow_encoder, cube_buffer, plane_buffer, patha_vertex_argument_buffers);
    }
    else
    {
        EncodeDrawItems(shadow_encoder, cube_buffer, plane_buffer, object_buffers, false, nullptr);
    }
    shadow_encoder->endEncoding();
    std::cout << "Pass 1/4 完成: Shadow Map 1024x1024 depth"
              << (scene_shader_path == SceneShaderPath::PathA ? " (Path A)" : " (legacy MSL)")
              << "\n";

    MTL::RenderPassDescriptor* hdr_pass = MTL::RenderPassDescriptor::alloc()->init();
    hdr_pass->colorAttachments()->object(0)->setTexture(hdr_texture);
    hdr_pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    hdr_pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    hdr_pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.04, 0.06, 0.10, 1.0));
    hdr_pass->depthAttachment()->setTexture(hdr_depth);
    hdr_pass->depthAttachment()->setLoadAction(MTL::LoadActionClear);
    hdr_pass->depthAttachment()->setStoreAction(MTL::StoreActionDontCare);
    hdr_pass->depthAttachment()->setClearDepth(1.0);
    MTL::RenderCommandEncoder* hdr_encoder = command_buffer->renderCommandEncoder(hdr_pass);
    hdr_encoder->setRenderPipelineState(scene_pipeline);
    hdr_encoder->setDepthStencilState(scene_depth_state);
    hdr_encoder->setCullMode(MTL::CullModeNone);
    hdr_encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
    if (scene_shader_path == SceneShaderPath::PathA)
    {
        EncodeSceneDrawItemsPathA(hdr_encoder, cube_buffer, plane_buffer, patha_vertex_argument_buffers, patha_fragment_argument_buffers);
    }
    else
    {
        hdr_encoder->setFragmentTexture(shadow_texture, 0);
        hdr_encoder->setFragmentSamplerState(shadow_sampler, 0);
        EncodeDrawItems(hdr_encoder, cube_buffer, plane_buffer, object_buffers, true, scene_buffer);
    }
    hdr_encoder->endEncoding();
    std::cout << "Pass 2/4 完成: HDR 场景 + 阴影采样"
              << (scene_shader_path == SceneShaderPath::PathA ? " (Path A)" : " (legacy MSL)")
              << "\n";

    auto encode_fullscreen_pass =
        [&](MTL::Texture* target,
            MTL::RenderPipelineState* pipeline,
            MTL::Texture* input0,
            MTL::Texture* input1,
            bool bind_scene_buffer)
        {
            MTL::RenderPassDescriptor* pass = MTL::RenderPassDescriptor::alloc()->init();
            pass->colorAttachments()->object(0)->setTexture(target);
            pass->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
            pass->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
            pass->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.0, 0.0, 0.0, 1.0));
            MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass);
            encoder->setRenderPipelineState(pipeline);
            if (bind_scene_buffer)
            {
                encoder->setFragmentBuffer(scene_buffer, 0, 0);
            }
            if (input0 != nullptr)
            {
                encoder->setFragmentTexture(input0, 0);
            }
            if (input1 != nullptr)
            {
                encoder->setFragmentTexture(input1, 1);
            }
            encoder->setFragmentSamplerState(linear_sampler, 0);
            encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
            encoder->endEncoding();
        };

    encode_fullscreen_pass(bright_texture, bright_pipeline, hdr_texture, nullptr, false);
    encode_fullscreen_pass(blur_ping_texture, blur_h_pipeline, bright_texture, nullptr, false);
    encode_fullscreen_pass(blur_pong_texture, blur_v_pipeline, blur_ping_texture, nullptr, false);
    std::cout << "Pass 3/4 完成: 高亮提取 + Bloom 横纵向 blur\n";

    encode_fullscreen_pass(final_texture, composite_pipeline, hdr_texture, blur_pong_texture, true);
    std::cout << "Pass 4/4 完成: Tone Mapping + Bloom 合成\n";

    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    std::vector<Pixel> pixels(kWidth * kHeight);
    final_texture->getBytes(pixels.data(),
                            static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
                            MTL::Region::Make2D(0, 0, kWidth, kHeight),
                            0);

    std::filesystem::create_directories("out");
    const std::filesystem::path output_path = std::filesystem::path("out") / "advanced_lighting.ppm";
    if (!WritePpm(output_path, pixels))
    {
        std::cerr << "无法写出 D6 渲染结果: " << output_path << "\n";
        pool->drain();
        return 1;
    }

    std::cout << "D6 离屏渲染完成: " << output_path << "\n";
    pool->drain();
    return 0;
}
