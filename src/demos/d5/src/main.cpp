#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

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
constexpr std::uint32_t kSceneWidth = 768;
constexpr std::uint32_t kSceneHeight = 768;
constexpr NS::UInteger kShowcaseMsaaSampleCount = 4;

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

struct alignas(16) PlaneUniformData
{
    float mvpMatrix[16];
    float modelMatrix[16];
    float lightPos[3];
    float normalStrength;
    float cameraPos[3];
    float reflectionStrength;
    float tint[3];
    float _pad0;
};

static_assert(sizeof(PlaneUniformData) == 176, "PlaneUniformData 大小必须为 176 字节");

struct PlaneVertex
{
    float position[3];
    float normal[3];
    float tangent[3];
    float uv[2];
};

constexpr PlaneVertex kPlaneVertices[] = {
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.0f, -1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
};

constexpr NS::UInteger kPlaneVertexCount = 6;

struct PanelSetup
{
    float translateX;
    float translateY;
    float translateZ;
    float rotateX;
    float rotateY;
    float scaleX;
    float scaleY;
    float normalStrength;
    float reflectionStrength;
    Float3 tint;
};

constexpr PanelSetup kPanels[] = {
    {-1.02f, -0.58f, 0.12f, -0.98f,  0.28f, 0.92f, 0.92f, 0.0f,  0.10f, {0.92f, 0.98f, 1.00f}},
    { 1.05f, -0.54f, 0.05f, -1.06f, -0.42f, 0.92f, 0.92f, 1.35f, 0.24f, {1.00f, 0.98f, 0.95f}},
};

const char* kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct PlaneUniformData
{
    float4x4 mvpMatrix;
    float4x4 modelMatrix;
    packed_float3 lightPos;
    float normalStrength;
    packed_float3 cameraPos;
    float reflectionStrength;
    packed_float3 tint;
    float _pad0;
};

struct PlaneVertexIn
{
    float3 position [[attribute(0)]];
    float3 normal [[attribute(1)]];
    float3 tangent [[attribute(2)]];
    float2 uv [[attribute(3)]];
};

struct PlaneVertexOut
{
    float4 position [[position]];
    float3 worldPos;
    float3 worldNormal;
    float3 worldTangent;
    float3 worldBitangent;
    float2 uv;
};

struct SkyVertexOut
{
    float4 position [[position]];
    float2 ndc;
};

vertex SkyVertexOut skyVertex(uint vertexId [[vertex_id]])
{
    float2 positions[3] = {
        float2(-1.0, -1.0),
        float2( 3.0, -1.0),
        float2(-1.0,  3.0)
    };

    SkyVertexOut out;
    out.position = float4(positions[vertexId], 0.0, 1.0);
    out.ndc = positions[vertexId];
    return out;
}

fragment float4 skyFragment(SkyVertexOut in [[stage_in]],
                            texturecube<float> skyTexture [[texture(0)]],
                            sampler skySampler [[sampler(0)]])
{
    float2 uv = in.ndc * 0.5 + 0.5;
    float2 centered = float2(uv.x * 2.0 - 1.0, 1.0 - uv.y * 2.0);
    float3 dir = normalize(float3(centered.x * 1.28, centered.y * 0.94 - 0.08, 1.0));
    return float4(skyTexture.sample(skySampler, dir).rgb, 1.0);
}

vertex PlaneVertexOut planeVertex(PlaneVertexIn in [[stage_in]],
                                  constant PlaneUniformData& uniforms [[buffer(1)]])
{
    PlaneVertexOut out;
    float4 worldPos4 = uniforms.modelMatrix * float4(in.position, 1.0);
    float3x3 normalMatrix = float3x3(
        uniforms.modelMatrix[0].xyz,
        uniforms.modelMatrix[1].xyz,
        uniforms.modelMatrix[2].xyz);

    float3 worldNormal = normalize(normalMatrix * in.normal);
    float3 worldTangent = normalize(normalMatrix * in.tangent);
    float3 worldBitangent = normalize(cross(worldNormal, worldTangent));

    out.position = uniforms.mvpMatrix * float4(in.position, 1.0);
    out.worldPos = worldPos4.xyz;
    out.worldNormal = worldNormal;
    out.worldTangent = worldTangent;
    out.worldBitangent = worldBitangent;
    out.uv = in.uv;
    return out;
}

fragment float4 planeFragment(PlaneVertexOut in [[stage_in]],
                              constant PlaneUniformData& uniforms [[buffer(0)]],
                              texture2d<float> albedoTexture [[texture(0)]],
                              texture2d<float> normalTexture [[texture(1)]],
                              texturecube<float> skyTexture [[texture(2)]],
                              sampler planeSampler [[sampler(0)]],
                              sampler skySampler [[sampler(1)]])
{
    float2 tiledUv = in.uv * 3.0;
    float3 albedo = albedoTexture.sample(planeSampler, tiledUv).rgb * uniforms.tint;
    float3 tangentNormal = normalTexture.sample(planeSampler, tiledUv).rgb * 2.0 - 1.0;
    tangentNormal.xy *= uniforms.normalStrength;
    tangentNormal.z = max(tangentNormal.z, 0.08);
    tangentNormal = normalize(tangentNormal);

    float3x3 tbn = float3x3(
        normalize(in.worldTangent),
        normalize(in.worldBitangent),
        normalize(in.worldNormal));
    float3 N = normalize(tbn * tangentNormal);
    float3 L = normalize(uniforms.lightPos - in.worldPos);
    float3 V = normalize(uniforms.cameraPos - in.worldPos);
    float3 H = normalize(L + V);
    float3 R = reflect(-V, N);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(N, H), 0.0), 42.0);
    float fresnel = pow(1.0 - max(dot(N, V), 0.0), 4.0);
    float3 env = skyTexture.sample(skySampler, R).rgb;

    float3 ambient = albedo * 0.22;
    float3 diffuse = albedo * diff;
    float3 specular = float3(0.38, 0.35, 0.33) * spec;
    float3 reflected = env * (uniforms.reflectionStrength + fresnel * 0.18);
    return float4(ambient + diffuse + specular + reflected, 1.0);
}
)";

std::string ErrorToString(NS::Error* error)
{
    if (error == nullptr)
    {
        return "未知错误";
    }

    NS::String* description = error->localizedDescription();
    return description != nullptr ? description->utf8String() : "未知错误";
}

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

Float3 Add(const Float3& a, const Float3& b)
{
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Float3 Scale(const Float3& v, float s)
{
    return {v.x * s, v.y * s, v.z * s};
}

Float3 Lerp(const Float3& a, const Float3& b, float t)
{
    return {
        a.x + (b.x - a.x) * t,
        a.y + (b.y - a.y) * t,
        a.z + (b.z - a.z) * t,
    };
}

float Dot(const Float3& a, const Float3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Float3 Normalize(const Float3& value)
{
    const float length = std::sqrt(std::max(Dot(value, value), 1.0e-8f));
    return {value.x / length, value.y / length, value.z / length};
}

std::uint8_t ToByte(float value)
{
    return static_cast<std::uint8_t>(Clamp01(value) * 255.0f + 0.5f);
}

bool WritePpm(const std::filesystem::path& output_path,
              std::uint32_t width,
              std::uint32_t height,
              const std::vector<Pixel>& pixels)
{
    std::ofstream output(output_path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P6\n" << width << " " << height << "\n255\n";
    for (const Pixel& pixel : pixels)
    {
        output.put(static_cast<char>(pixel.r));
        output.put(static_cast<char>(pixel.g));
        output.put(static_cast<char>(pixel.b));
    }

    return true;
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

Mat4 Mat4Perspective(float fov_y, float aspect, float near_z, float far_z)
{
    const float tan_half = std::tan(fov_y * 0.5f);
    Mat4 result;
    result.m[0] = 1.0f / (aspect * tan_half);
    result.m[5] = 1.0f / tan_half;
    result.m[10] = far_z / (near_z - far_z);
    result.m[11] = -1.0f;
    result.m[14] = near_z * far_z / (near_z - far_z);
    return result;
}

Mat4 Mat4LookAt(float eye_x, float eye_y, float eye_z,
                float target_x, float target_y, float target_z,
                float up_x, float up_y, float up_z)
{
    float fx = target_x - eye_x;
    float fy = target_y - eye_y;
    float fz = target_z - eye_z;
    const float forward_length = std::sqrt(fx * fx + fy * fy + fz * fz);
    fx /= forward_length;
    fy /= forward_length;
    fz /= forward_length;

    float sx = fy * up_z - fz * up_y;
    float sy = fz * up_x - fx * up_z;
    float sz = fx * up_y - fy * up_x;
    const float side_length = std::sqrt(sx * sx + sy * sy + sz * sz);
    sx /= side_length;
    sy /= side_length;
    sz /= side_length;

    const float ux = sy * fz - sz * fy;
    const float uy = sz * fx - sx * fz;
    const float uz = sx * fy - sy * fx;

    Mat4 result;
    result.m[0] = sx;
    result.m[4] = sy;
    result.m[8] = sz;
    result.m[12] = -(sx * eye_x + sy * eye_y + sz * eye_z);
    result.m[1] = ux;
    result.m[5] = uy;
    result.m[9] = uz;
    result.m[13] = -(ux * eye_x + uy * eye_y + uz * eye_z);
    result.m[2] = -fx;
    result.m[6] = -fy;
    result.m[10] = -fz;
    result.m[14] = (fx * eye_x + fy * eye_y + fz * eye_z);
    result.m[3] = 0.0f;
    result.m[7] = 0.0f;
    result.m[11] = 0.0f;
    result.m[15] = 1.0f;
    return result;
}

Mat4 Mat4RotateX(float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result;
    result.m[0] = 1.0f;
    result.m[5] = c;
    result.m[6] = s;
    result.m[9] = -s;
    result.m[10] = c;
    result.m[15] = 1.0f;
    return result;
}

Mat4 Mat4RotateY(float angle)
{
    const float c = std::cos(angle);
    const float s = std::sin(angle);
    Mat4 result;
    result.m[0] = c;
    result.m[2] = -s;
    result.m[5] = 1.0f;
    result.m[8] = s;
    result.m[10] = c;
    result.m[15] = 1.0f;
    return result;
}

Mat4 Mat4Scale(float x, float y, float z)
{
    Mat4 result;
    result.m[0] = x;
    result.m[5] = y;
    result.m[10] = z;
    result.m[15] = 1.0f;
    return result;
}

Mat4 Mat4Translate(float x, float y, float z)
{
    Mat4 result;
    result.m[0] = 1.0f;
    result.m[5] = 1.0f;
    result.m[10] = 1.0f;
    result.m[12] = x;
    result.m[13] = y;
    result.m[14] = z;
    result.m[15] = 1.0f;
    return result;
}

Mat4 BuildPanelModelMatrix(const PanelSetup& panel)
{
    return Mat4Mul(
        Mat4Translate(panel.translateX, panel.translateY, panel.translateZ),
        Mat4Mul(
            Mat4RotateY(panel.rotateY),
            Mat4Mul(Mat4RotateX(panel.rotateX), Mat4Scale(panel.scaleX, panel.scaleY, 1.0f))));
}

PlaneUniformData BuildPanelUniformData(const Mat4& view_projection, const PanelSetup& panel)
{
    const Mat4 model = BuildPanelModelMatrix(panel);
    const Mat4 mvp = Mat4Mul(view_projection, model);

    PlaneUniformData uniform_data = {};
    std::memcpy(uniform_data.mvpMatrix, mvp.m, sizeof(mvp.m));
    std::memcpy(uniform_data.modelMatrix, model.m, sizeof(model.m));
    uniform_data.lightPos[0] = 2.1f;
    uniform_data.lightPos[1] = 1.85f;
    uniform_data.lightPos[2] = 2.8f;
    uniform_data.normalStrength = panel.normalStrength;
    uniform_data.cameraPos[0] = 0.0f;
    uniform_data.cameraPos[1] = 0.38f;
    uniform_data.cameraPos[2] = 4.35f;
    uniform_data.reflectionStrength = panel.reflectionStrength;
    uniform_data.tint[0] = panel.tint.x;
    uniform_data.tint[1] = panel.tint.y;
    uniform_data.tint[2] = panel.tint.z;
    return uniform_data;
}

MTL::Texture* CreateAlbedoTexture(MTL::Device* device)
{
    constexpr std::uint32_t texture_size = 160;
    std::vector<Pixel> pixels(texture_size * texture_size);

    for (std::uint32_t y = 0; y < texture_size; ++y)
    {
        for (std::uint32_t x = 0; x < texture_size; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(texture_size - 1);
            const float v = static_cast<float>(y) / static_cast<float>(texture_size - 1);
            const float tile_u = u * 6.0f;
            const float tile_v = v * 6.0f;
            const float frac_u = tile_u - std::floor(tile_u);
            const float frac_v = tile_v - std::floor(tile_v);
            const float seam_u = std::min(frac_u, 1.0f - frac_u);
            const float seam_v = std::min(frac_v, 1.0f - frac_v);
            const float seam = std::min(seam_u, seam_v);

            Float3 color = {0.77f, 0.62f, 0.42f};
            const float warm_variation = 0.86f + 0.14f * std::sin((u * 5.6f + v * 4.2f) * 6.28318f);
            color = Scale(color, warm_variation);

            const float patina = Clamp01(0.55f + 0.45f * std::sin(u * 6.28318f * 3.0f) * std::cos(v * 6.28318f * 2.0f));
            color = Lerp(color, {0.28f, 0.50f, 0.44f}, patina * 0.18f);

            const float seam_mask = Clamp01((0.06f - seam) / 0.06f);
            color = Lerp(color, {0.22f, 0.18f, 0.14f}, seam_mask * 0.85f);

            const bool bright_rivet = (static_cast<int>(std::floor(tile_u)) + static_cast<int>(std::floor(tile_v))) % 2 == 0;
            const float rivet_u = frac_u - 0.5f;
            const float rivet_v = frac_v - 0.5f;
            const float rivet_distance = std::sqrt(rivet_u * rivet_u + rivet_v * rivet_v);
            const float rivet_mask = Clamp01((0.28f - rivet_distance) / 0.10f);
            if (bright_rivet)
            {
                color = Lerp(color, {0.95f, 0.84f, 0.48f}, rivet_mask * 0.40f);
            }
            else
            {
                color = Lerp(color, {0.14f, 0.18f, 0.20f}, rivet_mask * 0.45f);
            }

            pixels[y * texture_size + x] = Pixel{ToByte(color.z), ToByte(color.y), ToByte(color.x), 255};
        }
    }

    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, texture_size, texture_size, false);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* texture = device->newTexture(descriptor);
    if (texture != nullptr)
    {
        texture->replaceRegion(MTL::Region::Make2D(0, 0, texture_size, texture_size), 0, pixels.data(), texture_size * sizeof(Pixel));
    }
    return texture;
}

MTL::Texture* CreateNormalTexture(MTL::Device* device)
{
    constexpr std::uint32_t texture_size = 160;
    std::vector<float> height(texture_size * texture_size, 0.0f);

    for (std::uint32_t y = 0; y < texture_size; ++y)
    {
        for (std::uint32_t x = 0; x < texture_size; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(texture_size - 1);
            const float v = static_cast<float>(y) / static_cast<float>(texture_size - 1);
            const float tile_u = u * 6.0f;
            const float tile_v = v * 6.0f;
            const float frac_u = tile_u - std::floor(tile_u);
            const float frac_v = tile_v - std::floor(tile_v);
            const float local_u = frac_u - 0.5f;
            const float local_v = frac_v - 0.5f;
            const float radius = std::sqrt(local_u * local_u + local_v * local_v);

            float value = 0.0f;
            if (radius < 0.30f)
            {
                const float t = 1.0f - radius / 0.30f;
                value += t * t * 0.95f;
            }

            const float seam_distance = std::min(std::min(frac_u, 1.0f - frac_u), std::min(frac_v, 1.0f - frac_v));
            value -= Clamp01((0.08f - seam_distance) / 0.08f) * 0.26f;
            value += 0.06f * std::sin(u * 6.28318f * 8.0f);
            value += 0.05f * std::cos(v * 6.28318f * 6.0f);
            height[y * texture_size + x] = value;
        }
    }

    std::vector<Pixel> pixels(texture_size * texture_size);
    for (std::uint32_t y = 0; y < texture_size; ++y)
    {
        for (std::uint32_t x = 0; x < texture_size; ++x)
        {
            const std::uint32_t left = x == 0 ? x : x - 1;
            const std::uint32_t right = x + 1 == texture_size ? x : x + 1;
            const std::uint32_t down = y == 0 ? y : y - 1;
            const std::uint32_t up = y + 1 == texture_size ? y : y + 1;

            const float dx = height[y * texture_size + right] - height[y * texture_size + left];
            const float dy = height[up * texture_size + x] - height[down * texture_size + x];

            Float3 normal = Normalize({-dx * 1.8f, -dy * 1.8f, 1.0f});
            pixels[y * texture_size + x] = Pixel{
                ToByte(normal.z * 0.5f + 0.5f),
                ToByte(normal.y * 0.5f + 0.5f),
                ToByte(normal.x * 0.5f + 0.5f),
                255};
        }
    }

    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, texture_size, texture_size, false);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* texture = device->newTexture(descriptor);
    if (texture != nullptr)
    {
        texture->replaceRegion(MTL::Region::Make2D(0, 0, texture_size, texture_size), 0, pixels.data(), texture_size * sizeof(Pixel));
    }
    return texture;
}

Float3 CubeDirectionForFace(NS::UInteger face, float u, float v)
{
    const float sx = u * 2.0f - 1.0f;
    const float sy = 1.0f - v * 2.0f;
    switch (face)
    {
    case 0: return Normalize({ 1.0f, sy, -sx});
    case 1: return Normalize({-1.0f, sy,  sx});
    case 2: return Normalize({ sx, 1.0f, -sy});
    case 3: return Normalize({ sx,-1.0f,  sy});
    case 4: return Normalize({ sx, sy,  1.0f});
    default: return Normalize({-sx, sy, -1.0f});
    }
}

Float3 SampleSkyColor(const Float3& dir)
{
    const Float3 deep_sky = {0.16f, 0.34f, 0.74f};
    const Float3 horizon_sky = {0.98f, 0.66f, 0.36f};
    const Float3 ground_near = {0.42f, 0.28f, 0.18f};
    const Float3 ground_far = {0.20f, 0.17f, 0.15f};
    const Float3 sun_direction = Normalize({0.36f, 0.28f, 0.89f});

    const float sky_t = Clamp01((dir.y + 0.12f) / 0.98f);
    Float3 color = Lerp(horizon_sky, deep_sky, sky_t * sky_t);

    if (dir.y < 0.0f)
    {
        const float ground_t = Clamp01(-dir.y / 0.95f);
        color = Lerp(ground_near, ground_far, ground_t);
    }

    const float sun_amount = std::pow(std::max(Dot(dir, sun_direction), 0.0f), 1800.0f);
    const float sun_glow = std::pow(std::max(Dot(dir, sun_direction), 0.0f), 96.0f);
    color = Add(color, Scale({1.00f, 0.88f, 0.60f}, sun_amount * 7.0f + sun_glow * 0.65f));

    if (dir.y > 0.0f)
    {
        const float cloud_wave =
            0.5f + 0.5f * std::sin(dir.x * 18.0f + dir.z * 11.0f + dir.y * 5.0f) *
            std::cos(dir.x * 7.0f - dir.z * 15.0f);
        const float cloud_mask = Clamp01((cloud_wave - 0.62f) / 0.18f) * Clamp01((dir.y + 0.05f) / 0.35f);
        color = Lerp(color, {0.96f, 0.94f, 0.93f}, cloud_mask * 0.28f);
    }

    const float azimuth = std::atan2(dir.x, dir.z);
    const float skyline = -0.05f + 0.03f * std::sin(azimuth * 5.0f) + 0.015f * std::sin(azimuth * 13.0f);
    if (dir.y < skyline && dir.y > skyline - 0.10f && dir.z > 0.0f)
    {
        color = Lerp(color, {0.10f, 0.10f, 0.13f}, 0.78f);
    }

    return {
        Clamp01(color.x),
        Clamp01(color.y),
        Clamp01(color.z),
    };
}

MTL::Texture* CreateSkyboxTexture(MTL::Device* device)
{
    constexpr std::uint32_t face_size = 96;
    MTL::TextureDescriptor* descriptor = MTL::TextureDescriptor::alloc()->init();
    descriptor->setTextureType(MTL::TextureTypeCube);
    descriptor->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    descriptor->setWidth(face_size);
    descriptor->setHeight(face_size);
    descriptor->setDepth(1);
    descriptor->setMipmapLevelCount(1);
    descriptor->setSampleCount(1);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);

    MTL::Texture* texture = device->newTexture(descriptor);
    if (texture == nullptr)
    {
        return nullptr;
    }

    for (NS::UInteger face = 0; face < 6; ++face)
    {
        std::vector<Pixel> pixels(face_size * face_size);
        for (std::uint32_t y = 0; y < face_size; ++y)
        {
            for (std::uint32_t x = 0; x < face_size; ++x)
            {
                const float u = static_cast<float>(x) / static_cast<float>(face_size - 1);
                const float v = static_cast<float>(y) / static_cast<float>(face_size - 1);
                const Float3 dir = CubeDirectionForFace(face, u, v);
                const Float3 color = SampleSkyColor(dir);
                pixels[y * face_size + x] = Pixel{ToByte(color.z), ToByte(color.y), ToByte(color.x), 255};
            }
        }

        texture->replaceRegion(MTL::Region::Make2D(0, 0, face_size, face_size),
                               0,
                               face,
                               pixels.data(),
                               face_size * sizeof(Pixel),
                               face_size * face_size * sizeof(Pixel));
    }

    return texture;
}

MTL::RenderPipelineState* BuildSkyPipeline(MTL::Device* device,
                                           MTL::Function* sky_vertex,
                                           MTL::Function* sky_fragment,
                                           NS::UInteger sample_count)
{
    NS::Error* error = nullptr;
    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(sky_vertex);
    descriptor->setFragmentFunction(sky_fragment);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    descriptor->setSampleCount(sample_count);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 Skybox 管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildPlanePipeline(MTL::Device* device,
                                             MTL::Function* plane_vertex,
                                             MTL::Function* plane_fragment,
                                             NS::UInteger sample_count)
{
    NS::Error* error = nullptr;
    MTL::VertexDescriptor* vertex_descriptor = MTL::VertexDescriptor::alloc()->init();
    vertex_descriptor->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    vertex_descriptor->attributes()->object(0)->setOffset(0);
    vertex_descriptor->attributes()->object(0)->setBufferIndex(0);
    vertex_descriptor->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
    vertex_descriptor->attributes()->object(1)->setOffset(sizeof(float) * 3);
    vertex_descriptor->attributes()->object(1)->setBufferIndex(0);
    vertex_descriptor->attributes()->object(2)->setFormat(MTL::VertexFormatFloat3);
    vertex_descriptor->attributes()->object(2)->setOffset(sizeof(float) * 6);
    vertex_descriptor->attributes()->object(2)->setBufferIndex(0);
    vertex_descriptor->attributes()->object(3)->setFormat(MTL::VertexFormatFloat2);
    vertex_descriptor->attributes()->object(3)->setOffset(sizeof(float) * 9);
    vertex_descriptor->attributes()->object(3)->setBufferIndex(0);
    vertex_descriptor->layouts()->object(0)->setStride(sizeof(PlaneVertex));

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(plane_vertex);
    descriptor->setFragmentFunction(plane_fragment);
    descriptor->setVertexDescriptor(vertex_descriptor);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    descriptor->setSampleCount(sample_count);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 Plane 管线: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

std::vector<Pixel> RenderScene(MTL::Device* device,
                               MTL::CommandQueue* command_queue,
                               MTL::Library* library,
                               MTL::Texture* albedo_texture,
                               MTL::Texture* normal_texture,
                               MTL::Texture* skybox_texture,
                               MTL::DepthStencilState* depth_stencil_state,
                               NS::UInteger sample_count)
{
    MTL::Function* sky_vertex = library->newFunction(MTLSTR("skyVertex"));
    MTL::Function* sky_fragment = library->newFunction(MTLSTR("skyFragment"));
    MTL::Function* plane_vertex = library->newFunction(MTLSTR("planeVertex"));
    MTL::Function* plane_fragment = library->newFunction(MTLSTR("planeFragment"));
    if (sky_vertex == nullptr || sky_fragment == nullptr || plane_vertex == nullptr || plane_fragment == nullptr)
    {
        std::cerr << "无法创建 D5 着色器入口函数。\n";
        return {};
    }

    MTL::RenderPipelineState* sky_pipeline = BuildSkyPipeline(device, sky_vertex, sky_fragment, sample_count);
    MTL::RenderPipelineState* plane_pipeline = BuildPlanePipeline(device, plane_vertex, plane_fragment, sample_count);
    if (sky_pipeline == nullptr || plane_pipeline == nullptr)
    {
        return {};
    }

    MTL::SamplerDescriptor* plane_sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    plane_sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
    plane_sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);
    plane_sampler_descriptor->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
    plane_sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeRepeat);
    plane_sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeRepeat);

    MTL::SamplerDescriptor* sky_sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    sky_sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
    sky_sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);
    sky_sampler_descriptor->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
    sky_sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sky_sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeClampToEdge);

    MTL::SamplerState* plane_sampler = device->newSamplerState(plane_sampler_descriptor);
    MTL::SamplerState* sky_sampler = device->newSamplerState(sky_sampler_descriptor);
    if (plane_sampler == nullptr || sky_sampler == nullptr)
    {
        std::cerr << "无法创建采样器状态。\n";
        return {};
    }

    MTL::Buffer* plane_vertex_buffer =
        device->newBuffer(kPlaneVertices, sizeof(kPlaneVertices), MTL::ResourceStorageModeShared);
    if (plane_vertex_buffer == nullptr)
    {
        std::cerr << "无法创建 D5 顶点缓冲。\n";
        return {};
    }

    const Mat4 view = Mat4LookAt(0.0f, 0.38f, 4.35f, 0.0f, -0.15f, 0.0f, 0.0f, 1.0f, 0.0f);
    const Mat4 proj = Mat4Perspective(3.14159f * 0.25f, static_cast<float>(kSceneWidth) / static_cast<float>(kSceneHeight), 0.1f, 100.0f);
    const Mat4 view_projection = Mat4Mul(proj, view);

    std::array<MTL::Buffer*, 2> panel_uniform_buffers = {};
    for (std::size_t index = 0; index < std::size(kPanels); ++index)
    {
        const PlaneUniformData uniform_data = BuildPanelUniformData(view_projection, kPanels[index]);
        panel_uniform_buffers[index] = device->newBuffer(&uniform_data, sizeof(PlaneUniformData), MTL::ResourceStorageModeShared);
        if (panel_uniform_buffers[index] == nullptr)
        {
            std::cerr << "无法创建 D5 Uniform Buffer。\n";
            return {};
        }
    }

    MTL::TextureDescriptor* resolve_descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kSceneWidth, kSceneHeight, false);
    resolve_descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    resolve_descriptor->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* resolve_texture = device->newTexture(resolve_descriptor);

    MTL::Texture* color_texture = resolve_texture;
    if (sample_count > 1)
    {
        MTL::TextureDescriptor* msaa_color_descriptor =
            MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kSceneWidth, kSceneHeight, false);
        msaa_color_descriptor->setTextureType(MTL::TextureType2DMultisample);
        msaa_color_descriptor->setSampleCount(sample_count);
        msaa_color_descriptor->setUsage(MTL::TextureUsageRenderTarget);
        msaa_color_descriptor->setStorageMode(MTL::StorageModePrivate);
        color_texture = device->newTexture(msaa_color_descriptor);
    }

    MTL::TextureDescriptor* depth_descriptor = MTL::TextureDescriptor::alloc()->init();
    depth_descriptor->setTextureType(sample_count > 1 ? MTL::TextureType2DMultisample : MTL::TextureType2D);
    depth_descriptor->setPixelFormat(MTL::PixelFormatDepth32Float);
    depth_descriptor->setWidth(kSceneWidth);
    depth_descriptor->setHeight(kSceneHeight);
    depth_descriptor->setDepth(1);
    depth_descriptor->setSampleCount(sample_count);
    depth_descriptor->setUsage(MTL::TextureUsageRenderTarget);
    depth_descriptor->setStorageMode(MTL::StorageModePrivate);
    MTL::Texture* depth_texture = device->newTexture(depth_descriptor);
    if (resolve_texture == nullptr || color_texture == nullptr || depth_texture == nullptr)
    {
        std::cerr << "无法创建 D5 渲染目标纹理。\n";
        return {};
    }

    MTL::RenderPassDescriptor* pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* color_attachment = pass_descriptor->colorAttachments()->object(0);
    color_attachment->setTexture(color_texture);
    color_attachment->setLoadAction(MTL::LoadActionClear);
    color_attachment->setClearColor(MTL::ClearColor(0.03, 0.05, 0.08, 1.0));
    if (sample_count > 1)
    {
        color_attachment->setResolveTexture(resolve_texture);
        color_attachment->setStoreAction(MTL::StoreActionMultisampleResolve);
    }
    else
    {
        color_attachment->setStoreAction(MTL::StoreActionStore);
    }

    MTL::RenderPassDepthAttachmentDescriptor* depth_attachment = pass_descriptor->depthAttachment();
    depth_attachment->setTexture(depth_texture);
    depth_attachment->setLoadAction(MTL::LoadActionClear);
    depth_attachment->setStoreAction(MTL::StoreActionDontCare);
    depth_attachment->setClearDepth(1.0);

    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass_descriptor);

    encoder->setRenderPipelineState(sky_pipeline);
    encoder->setFragmentTexture(skybox_texture, 0);
    encoder->setFragmentSamplerState(sky_sampler, 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));

    encoder->setRenderPipelineState(plane_pipeline);
    encoder->setDepthStencilState(depth_stencil_state);
    encoder->setCullMode(MTL::CullModeBack);
    encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
    encoder->setVertexBuffer(plane_vertex_buffer, 0, 0);
    encoder->setFragmentTexture(albedo_texture, 0);
    encoder->setFragmentTexture(normal_texture, 1);
    encoder->setFragmentTexture(skybox_texture, 2);
    encoder->setFragmentSamplerState(plane_sampler, 0);
    encoder->setFragmentSamplerState(sky_sampler, 1);

    for (std::size_t index = 0; index < std::size(panel_uniform_buffers); ++index)
    {
        encoder->setVertexBuffer(panel_uniform_buffers[index], 0, 1);
        encoder->setFragmentBuffer(panel_uniform_buffers[index], 0, 0);
        encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), kPlaneVertexCount);
    }
    encoder->endEncoding();

    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    std::vector<Pixel> pixels(kSceneWidth * kSceneHeight);
    resolve_texture->getBytes(
        pixels.data(),
        static_cast<NS::UInteger>(kSceneWidth * sizeof(Pixel)),
        MTL::Region::Make2D(0, 0, kSceneWidth, kSceneHeight),
        0);

    return pixels;
}

std::vector<Pixel> BuildMsaaCompareZoom(const std::vector<Pixel>& left, const std::vector<Pixel>& right)
{
    constexpr std::uint32_t kCropSize = 192;
    constexpr std::uint32_t kScale = 4;
    constexpr std::uint32_t kCropX = 280;
    constexpr std::uint32_t kCropY = 280;

    std::vector<Pixel> output(kSceneWidth * 2 * kSceneHeight);
    auto sample_from = [](const std::vector<Pixel>& pixels, std::uint32_t source_x, std::uint32_t source_y) -> Pixel
    {
        const std::uint32_t clamped_x = std::min(source_x, kSceneWidth - 1);
        const std::uint32_t clamped_y = std::min(source_y, kSceneHeight - 1);
        return pixels[clamped_y * kSceneWidth + clamped_x];
    };

    for (std::uint32_t y = 0; y < kSceneHeight; ++y)
    {
        const std::uint32_t source_y = kCropY + std::min(y / kScale, kCropSize - 1);
        for (std::uint32_t x = 0; x < kSceneWidth; ++x)
        {
            const std::uint32_t source_x = kCropX + std::min(x / kScale, kCropSize - 1);
            output[y * (kSceneWidth * 2) + x] = sample_from(left, source_x, source_y);
            output[y * (kSceneWidth * 2) + kSceneWidth + x] = sample_from(right, source_x, source_y);
        }
    }
    return output;
}

struct DifferenceStats
{
    std::uint64_t changedPixels = 0;
    double averageChannelDifference = 0.0;
};

DifferenceStats ComputeDifferenceStats(const std::vector<Pixel>& a, const std::vector<Pixel>& b)
{
    DifferenceStats stats;
    double total_difference = 0.0;
    for (std::size_t index = 0; index < a.size() && index < b.size(); ++index)
    {
        const int dr = std::abs(static_cast<int>(a[index].r) - static_cast<int>(b[index].r));
        const int dg = std::abs(static_cast<int>(a[index].g) - static_cast<int>(b[index].g));
        const int db = std::abs(static_cast<int>(a[index].b) - static_cast<int>(b[index].b));
        const int diff = dr + dg + db;
        total_difference += static_cast<double>(diff) / 3.0;
        if (diff >= 6)
        {
            ++stats.changedPixels;
        }
    }

    if (!a.empty())
    {
        stats.averageChannelDifference = total_difference / static_cast<double>(a.size());
    }

    return stats;
}

} // namespace

int main()
{
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

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
    MTL::Library* library = device->newLibrary(source, nullptr, &error);
    if (library == nullptr)
    {
        std::cerr << "MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "MSL 着色器编译通过\n";

    MTL::Texture* albedo_texture = CreateAlbedoTexture(device);
    MTL::Texture* normal_texture = CreateNormalTexture(device);
    MTL::Texture* skybox_texture = CreateSkyboxTexture(device);
    if (albedo_texture == nullptr || normal_texture == nullptr || skybox_texture == nullptr)
    {
        std::cerr << "无法创建 D5 所需纹理。\n";
        pool->drain();
        return 1;
    }
    std::cout << "纹理已创建: albedo=160x160, normal=160x160, cubemap=96x96x6\n";

    MTL::DepthStencilDescriptor* depth_stencil_descriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
    depth_stencil_descriptor->setDepthWriteEnabled(true);
    MTL::DepthStencilState* depth_stencil_state = device->newDepthStencilState(depth_stencil_descriptor);
    if (depth_stencil_state == nullptr)
    {
        std::cerr << "无法创建深度状态对象。\n";
        pool->drain();
        return 1;
    }

    std::vector<Pixel> showcase_pixels = RenderScene(
        device,
        command_queue,
        library,
        albedo_texture,
        normal_texture,
        skybox_texture,
        depth_stencil_state,
        kShowcaseMsaaSampleCount);
    if (showcase_pixels.empty())
    {
        pool->drain();
        return 1;
    }

    std::vector<Pixel> noaa_pixels = RenderScene(
        device,
        command_queue,
        library,
        albedo_texture,
        normal_texture,
        skybox_texture,
        depth_stencil_state,
        1);
    if (noaa_pixels.empty())
    {
        pool->drain();
        return 1;
    }

    const std::vector<Pixel> compare_pixels = BuildMsaaCompareZoom(noaa_pixels, showcase_pixels);
    const DifferenceStats compare_stats = ComputeDifferenceStats(noaa_pixels, showcase_pixels);

    std::filesystem::create_directories("out");
    const std::filesystem::path showcase_path = std::filesystem::path("out") / "advanced_texturing_showcase.ppm";
    const std::filesystem::path compare_path = std::filesystem::path("out") / "advanced_texturing_msaa_compare.ppm";

    if (!WritePpm(showcase_path, kSceneWidth, kSceneHeight, showcase_pixels))
    {
        std::cerr << "无法写出 D5 展示图: " << showcase_path << "\n";
        pool->drain();
        return 1;
    }
    if (!WritePpm(compare_path, kSceneWidth * 2, kSceneHeight, compare_pixels))
    {
        std::cerr << "无法写出 D5 MSAA 对比图: " << compare_path << "\n";
        pool->drain();
        return 1;
    }

    std::cout << "D5 增强绘制完成: 左侧平面关闭法线扰动, 右侧平面开启法线贴图\n";
    std::cout << "天空盒已切换为连续天空/地平线 Cubemap\n";
    std::cout << "MSAA 对比已导出: 左=1x, 右=4x, 差异像素="
              << compare_stats.changedPixels
              << ", 平均通道差=" << compare_stats.averageChannelDifference << "\n";
    std::cout << "D5 离屏渲染完成: " << showcase_path << " + " << compare_path << "\n";

    pool->drain();
    return 0;
}
