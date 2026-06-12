#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t kWidth = 768;
constexpr std::uint32_t kHeight = 768;
constexpr NS::UInteger kMsaaSampleCount = 4;

struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

struct alignas(16) Mat4
{
    float m[16] = {};
};

struct alignas(16) UniformData
{
    float mvpMatrix[16];
    float modelMatrix[16];
    float lightPos[3];
    float _pad0;
    float cameraPos[3];
    float _pad1;
};

static_assert(sizeof(UniformData) == 160, "UniformData 大小必须为 160 字节");

struct PlaneVertex
{
    float position[3];
    float normal[3];
    float tangent[3];
    float uv[2];
};

constexpr PlaneVertex kPlaneVertices[] = {
    {{-1.35f, -1.35f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 1.35f, -1.35f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f}},
    {{ 1.35f,  1.35f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.35f, -1.35f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 1.0f}},
    {{ 1.35f,  1.35f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 0.0f}},
    {{-1.35f,  1.35f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
};

constexpr NS::UInteger kPlaneVertexCount = 6;

const char* kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct UniformData
{
    float4x4 mvpMatrix;
    float4x4 modelMatrix;
    packed_float3 lightPos;
    packed_float3 cameraPos;
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
    float2 centered = uv * 2.0 - 1.0;
    float3 dir = normalize(float3(centered.x, -centered.y, 1.15));
    return skyTexture.sample(skySampler, dir);
}

vertex PlaneVertexOut planeVertex(PlaneVertexIn in [[stage_in]],
                                  constant UniformData& uniforms [[buffer(1)]])
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
                              constant UniformData& uniforms [[buffer(0)]],
                              texture2d<float> albedoTexture [[texture(0)]],
                              texture2d<float> normalTexture [[texture(1)]],
                              texturecube<float> skyTexture [[texture(2)]],
                              sampler planeSampler [[sampler(0)]],
                              sampler skySampler [[sampler(1)]])
{
    float3 albedo = albedoTexture.sample(planeSampler, in.uv * 3.5).rgb;
    float3 tangentNormal = normalTexture.sample(planeSampler, in.uv * 3.5).rgb * 2.0 - 1.0;

    float3x3 tbn = float3x3(
        normalize(in.worldTangent),
        normalize(in.worldBitangent),
        normalize(in.worldNormal));
    float3 N = normalize(tbn * tangentNormal);
    float3 L = normalize(uniforms.lightPos - in.worldPos);
    float3 V = normalize(uniforms.cameraPos - in.worldPos);
    float3 R = reflect(-V, N);

    float diff = max(dot(N, L), 0.0);
    float spec = pow(max(dot(reflect(-L, N), V), 0.0), 32.0);
    float3 env = skyTexture.sample(skySampler, R).rgb;

    float3 ambient = albedo * 0.16;
    float3 diffuse = albedo * diff;
    float3 specular = float3(0.35) * spec;
    float3 reflected = env * 0.22;
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

bool WritePpm(const std::filesystem::path& output_path, const std::vector<Pixel>& pixels)
{
    std::ofstream output(output_path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P6\n" << kWidth << " " << kHeight << "\n255\n";
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

UniformData BuildUniformData()
{
    const Mat4 model = Mat4Mul(Mat4Translate(0.0f, -0.12f, 0.0f), Mat4Mul(Mat4RotateY(-0.55f), Mat4RotateX(-0.92f)));
    const Mat4 view = Mat4LookAt(0.0f, 0.2f, 3.1f, 0.0f, -0.1f, 0.0f, 0.0f, 1.0f, 0.0f);
    const Mat4 proj = Mat4Perspective(3.14159f * 0.34f, static_cast<float>(kWidth) / static_cast<float>(kHeight), 0.1f, 100.0f);
    const Mat4 mvp = Mat4Mul(Mat4Mul(proj, view), model);

    UniformData uniform_data = {};
    std::memcpy(uniform_data.mvpMatrix, mvp.m, sizeof(mvp.m));
    std::memcpy(uniform_data.modelMatrix, model.m, sizeof(model.m));
    uniform_data.lightPos[0] = 1.8f;
    uniform_data.lightPos[1] = 1.4f;
    uniform_data.lightPos[2] = 2.2f;
    uniform_data.cameraPos[0] = 0.0f;
    uniform_data.cameraPos[1] = 0.2f;
    uniform_data.cameraPos[2] = 3.1f;
    return uniform_data;
}

MTL::Texture* CreateAlbedoTexture(MTL::Device* device)
{
    constexpr std::uint32_t texture_size = 128;
    std::vector<Pixel> pixels(texture_size * texture_size);
    for (std::uint32_t y = 0; y < texture_size; ++y)
    {
        for (std::uint32_t x = 0; x < texture_size; ++x)
        {
            const bool even = ((x / 16) + (y / 16)) % 2 == 0;
            const std::uint8_t r = even ? 204 : 96;
            const std::uint8_t g = even ? 157 : 82;
            const std::uint8_t b = even ? 88 : 48;
            pixels[y * texture_size + x] = Pixel{b, g, r, 255};
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
    constexpr std::uint32_t texture_size = 128;
    std::vector<Pixel> pixels(texture_size * texture_size);
    for (std::uint32_t y = 0; y < texture_size; ++y)
    {
        for (std::uint32_t x = 0; x < texture_size; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(texture_size - 1);
            const float v = static_cast<float>(y) / static_cast<float>(texture_size - 1);
            const float sx = std::sin(u * 6.28318f * 4.0f);
            const float sy = std::cos(v * 6.28318f * 4.0f);
            float nx = sx * 0.55f;
            float ny = sy * 0.55f;
            float nz = 1.0f;
            const float inv_length = 1.0f / std::sqrt(nx * nx + ny * ny + nz * nz);
            nx *= inv_length;
            ny *= inv_length;
            nz *= inv_length;
            const std::uint8_t r = static_cast<std::uint8_t>((nx * 0.5f + 0.5f) * 255.0f);
            const std::uint8_t g = static_cast<std::uint8_t>((ny * 0.5f + 0.5f) * 255.0f);
            const std::uint8_t b = static_cast<std::uint8_t>((nz * 0.5f + 0.5f) * 255.0f);
            pixels[y * texture_size + x] = Pixel{b, g, r, 255};
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

MTL::Texture* CreateSkyboxTexture(MTL::Device* device)
{
    constexpr std::uint32_t face_size = 64;
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

    const std::array<std::array<float, 3>, 6> face_colors = {{
        {0.84f, 0.28f, 0.22f},
        {0.18f, 0.74f, 0.34f},
        {0.28f, 0.54f, 0.92f},
        {0.86f, 0.78f, 0.28f},
        {0.72f, 0.34f, 0.86f},
        {0.22f, 0.76f, 0.82f},
    }};

    for (NS::UInteger face = 0; face < 6; ++face)
    {
        std::vector<Pixel> pixels(face_size * face_size);
        for (std::uint32_t y = 0; y < face_size; ++y)
        {
            for (std::uint32_t x = 0; x < face_size; ++x)
            {
                const float u = static_cast<float>(x) / static_cast<float>(face_size - 1);
                const float v = static_cast<float>(y) / static_cast<float>(face_size - 1);
                const float mix = 0.35f + 0.65f * (0.55f * u + 0.45f * (1.0f - v));
                const std::uint8_t r = static_cast<std::uint8_t>(std::clamp(face_colors[face][0] * mix, 0.0f, 1.0f) * 255.0f);
                const std::uint8_t g = static_cast<std::uint8_t>(std::clamp(face_colors[face][1] * mix, 0.0f, 1.0f) * 255.0f);
                const std::uint8_t b = static_cast<std::uint8_t>(std::clamp(face_colors[face][2] * mix, 0.0f, 1.0f) * 255.0f);
                pixels[y * face_size + x] = Pixel{b, g, r, 255};
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

    MTL::Function* sky_vertex = library->newFunction(MTLSTR("skyVertex"));
    MTL::Function* sky_fragment = library->newFunction(MTLSTR("skyFragment"));
    MTL::Function* plane_vertex = library->newFunction(MTLSTR("planeVertex"));
    MTL::Function* plane_fragment = library->newFunction(MTLSTR("planeFragment"));
    if (sky_vertex == nullptr || sky_fragment == nullptr || plane_vertex == nullptr || plane_fragment == nullptr)
    {
        std::cerr << "无法创建 D5 着色器入口函数。\n";
        pool->drain();
        return 1;
    }

    MTL::RenderPipelineDescriptor* sky_pipeline_descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    sky_pipeline_descriptor->setVertexFunction(sky_vertex);
    sky_pipeline_descriptor->setFragmentFunction(sky_fragment);
    sky_pipeline_descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    sky_pipeline_descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    sky_pipeline_descriptor->setSampleCount(kMsaaSampleCount);

    MTL::RenderPipelineState* sky_pipeline = device->newRenderPipelineState(sky_pipeline_descriptor, &error);
    if (sky_pipeline == nullptr)
    {
        std::cerr << "无法创建 Skybox 管线: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::VertexDescriptor* plane_vertex_descriptor = MTL::VertexDescriptor::alloc()->init();
    plane_vertex_descriptor->attributes()->object(0)->setFormat(MTL::VertexFormatFloat3);
    plane_vertex_descriptor->attributes()->object(0)->setOffset(0);
    plane_vertex_descriptor->attributes()->object(0)->setBufferIndex(0);
    plane_vertex_descriptor->attributes()->object(1)->setFormat(MTL::VertexFormatFloat3);
    plane_vertex_descriptor->attributes()->object(1)->setOffset(sizeof(float) * 3);
    plane_vertex_descriptor->attributes()->object(1)->setBufferIndex(0);
    plane_vertex_descriptor->attributes()->object(2)->setFormat(MTL::VertexFormatFloat3);
    plane_vertex_descriptor->attributes()->object(2)->setOffset(sizeof(float) * 6);
    plane_vertex_descriptor->attributes()->object(2)->setBufferIndex(0);
    plane_vertex_descriptor->attributes()->object(3)->setFormat(MTL::VertexFormatFloat2);
    plane_vertex_descriptor->attributes()->object(3)->setOffset(sizeof(float) * 9);
    plane_vertex_descriptor->attributes()->object(3)->setBufferIndex(0);
    plane_vertex_descriptor->layouts()->object(0)->setStride(sizeof(PlaneVertex));

    MTL::RenderPipelineDescriptor* plane_pipeline_descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    plane_pipeline_descriptor->setVertexFunction(plane_vertex);
    plane_pipeline_descriptor->setFragmentFunction(plane_fragment);
    plane_pipeline_descriptor->setVertexDescriptor(plane_vertex_descriptor);
    plane_pipeline_descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    plane_pipeline_descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);
    plane_pipeline_descriptor->setSampleCount(kMsaaSampleCount);

    MTL::RenderPipelineState* plane_pipeline = device->newRenderPipelineState(plane_pipeline_descriptor, &error);
    if (plane_pipeline == nullptr)
    {
        std::cerr << "无法创建 Plane 管线: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::Texture* albedo_texture = CreateAlbedoTexture(device);
    MTL::Texture* normal_texture = CreateNormalTexture(device);
    MTL::Texture* skybox_texture = CreateSkyboxTexture(device);
    if (albedo_texture == nullptr || normal_texture == nullptr || skybox_texture == nullptr)
    {
        std::cerr << "无法创建 D5 所需纹理。\n";
        pool->drain();
        return 1;
    }
    std::cout << "纹理已创建: albedo=128x128, normal=128x128, cubemap=64x64x6\n";

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
        pool->drain();
        return 1;
    }

    MTL::Buffer* plane_vertex_buffer =
        device->newBuffer(kPlaneVertices, sizeof(kPlaneVertices), MTL::ResourceStorageModeShared);
    const UniformData uniform_data = BuildUniformData();
    MTL::Buffer* uniform_buffer =
        device->newBuffer(&uniform_data, sizeof(UniformData), MTL::ResourceStorageModeShared);
    if (plane_vertex_buffer == nullptr || uniform_buffer == nullptr)
    {
        std::cerr << "无法创建顶点或 Uniform Buffer。\n";
        pool->drain();
        return 1;
    }

    MTL::TextureDescriptor* msaa_color_descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, false);
    msaa_color_descriptor->setTextureType(MTL::TextureType2DMultisample);
    msaa_color_descriptor->setSampleCount(kMsaaSampleCount);
    msaa_color_descriptor->setUsage(MTL::TextureUsageRenderTarget);
    msaa_color_descriptor->setStorageMode(MTL::StorageModePrivate);
    MTL::Texture* msaa_color_texture = device->newTexture(msaa_color_descriptor);

    MTL::TextureDescriptor* resolve_descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, false);
    resolve_descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    resolve_descriptor->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* resolve_texture = device->newTexture(resolve_descriptor);

    MTL::TextureDescriptor* depth_descriptor = MTL::TextureDescriptor::alloc()->init();
    depth_descriptor->setTextureType(MTL::TextureType2DMultisample);
    depth_descriptor->setPixelFormat(MTL::PixelFormatDepth32Float);
    depth_descriptor->setWidth(kWidth);
    depth_descriptor->setHeight(kHeight);
    depth_descriptor->setDepth(1);
    depth_descriptor->setSampleCount(kMsaaSampleCount);
    depth_descriptor->setUsage(MTL::TextureUsageRenderTarget);
    depth_descriptor->setStorageMode(MTL::StorageModePrivate);
    MTL::Texture* depth_texture = device->newTexture(depth_descriptor);
    if (msaa_color_texture == nullptr || resolve_texture == nullptr || depth_texture == nullptr)
    {
        std::cerr << "无法创建 MSAA 或深度纹理。\n";
        pool->drain();
        return 1;
    }
    std::cout << "MSAA 已启用: sampleCount=" << kMsaaSampleCount << "\n";

    MTL::DepthStencilDescriptor* depth_stencil_descriptor = MTL::DepthStencilDescriptor::alloc()->init();
    depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
    depth_stencil_descriptor->setDepthWriteEnabled(true);
    MTL::DepthStencilState* depth_stencil_state = device->newDepthStencilState(depth_stencil_descriptor);

    MTL::RenderPassDescriptor* pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* color_attachment = pass_descriptor->colorAttachments()->object(0);
    color_attachment->setTexture(msaa_color_texture);
    color_attachment->setResolveTexture(resolve_texture);
    color_attachment->setLoadAction(MTL::LoadActionClear);
    color_attachment->setStoreAction(MTL::StoreActionMultisampleResolve);
    color_attachment->setClearColor(MTL::ClearColor(0.06, 0.08, 0.12, 1.0));

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
    encoder->setVertexBuffer(uniform_buffer, 0, 1);
    encoder->setFragmentBuffer(uniform_buffer, 0, 0);
    encoder->setFragmentTexture(albedo_texture, 0);
    encoder->setFragmentTexture(normal_texture, 1);
    encoder->setFragmentTexture(skybox_texture, 2);
    encoder->setFragmentSamplerState(plane_sampler, 0);
    encoder->setFragmentSamplerState(sky_sampler, 1);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), kPlaneVertexCount);
    encoder->endEncoding();

    command_buffer->commit();
    command_buffer->waitUntilCompleted();
    std::cout << "D5 绘制完成: 法线贴图平面 + Skybox + 4x MSAA resolve\n";

    std::vector<Pixel> pixels(kWidth * kHeight);
    resolve_texture->getBytes(pixels.data(),
                              static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
                              MTL::Region::Make2D(0, 0, kWidth, kHeight),
                              0);

    std::filesystem::create_directories("out");
    const std::filesystem::path output_path = std::filesystem::path("out") / "advanced_texturing.ppm";
    if (!WritePpm(output_path, pixels))
    {
        std::cerr << "无法写出 D5 渲染结果: " << output_path << "\n";
        pool->drain();
        return 1;
    }

    std::cout << "D5 离屏渲染完成: " << output_path << "\n";

    pool->drain();
    return 0;
}
