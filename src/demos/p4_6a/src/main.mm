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
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t kWidth = 512;
constexpr std::uint32_t kHeight = 512;
constexpr std::uint32_t kAtlasSize = 16;
constexpr std::uint32_t kTileSize = 8;

struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

std::string ErrorToString(NS::Error* error)
{
    if (error == nullptr)
    {
        return "未知错误";
    }

    NS::String* description = error->localizedDescription();
    return description != nullptr ? description->utf8String() : "未知错误";
}

bool WritePpm(const std::filesystem::path& output_path,
              const std::vector<Pixel>& pixels,
              std::uint32_t width,
              std::uint32_t height)
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

void SetAtlasPixel(std::vector<Pixel>& atlas, std::uint32_t x, std::uint32_t y, Pixel pixel)
{
    atlas[y * kAtlasSize + x] = pixel;
}

std::vector<Pixel> BuildAtlas()
{
    std::vector<Pixel> atlas(kAtlasSize * kAtlasSize);
    for (std::uint32_t y = 0; y < kAtlasSize; ++y)
    {
        for (std::uint32_t x = 0; x < kAtlasSize; ++x)
        {
            const std::uint32_t tile_x = x / kTileSize;
            const std::uint32_t tile_y = y / kTileSize;
            const float fx = static_cast<float>(x % kTileSize) / static_cast<float>(kTileSize - 1);
            const float fy = static_cast<float>(y % kTileSize) / static_cast<float>(kTileSize - 1);

            Pixel pixel = { 0, 0, 0, 0 };
            if (tile_x == 0 && tile_y == 0)
            {
                const bool checker = ((x + y) % 2) == 0;
                pixel = checker ? Pixel{ 40, 70, 140, 255 } : Pixel{ 18, 34, 74, 255 };
            }
            else if (tile_x == 1 && tile_y == 0)
            {
                const bool band = ((x / 2) % 2) == 0;
                pixel = band ? Pixel{ 180, 200, 80, 255 } : Pixel{ 105, 140, 30, 255 };
            }
            else if (tile_x == 0 && tile_y == 1)
            {
                const bool diagonal = x >= y;
                pixel = diagonal ? Pixel{ 220, 120, 40, 255 } : Pixel{ 240, 190, 110, 255 };
            }
            else
            {
                const float dx = fx * 2.0f - 1.0f;
                const float dy = fy * 2.0f - 1.0f;
                const float radius = std::sqrt(dx * dx + dy * dy);
                const float alpha = std::clamp(1.15f - radius * 1.2f, 0.0f, 1.0f);
                const std::uint8_t a = static_cast<std::uint8_t>(std::lround(alpha * 255.0f));
                const std::uint8_t r = static_cast<std::uint8_t>(120 + 120 * std::clamp(1.0f - radius, 0.0f, 1.0f));
                const std::uint8_t g = static_cast<std::uint8_t>(80 + 140 * std::clamp(1.0f - std::abs(dx), 0.0f, 1.0f));
                const std::uint8_t b = static_cast<std::uint8_t>(210 + 40 * std::clamp(1.0f - std::abs(dy), 0.0f, 1.0f));
                pixel = Pixel{ b, g, r, a };
            }

            SetAtlasPixel(atlas, x, y, pixel);
        }
    }

    return atlas;
}

MTL::Texture* CreateAtlasTexture(MTL::Device* device, const std::vector<Pixel>& atlas_pixels)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kAtlasSize, kAtlasSize, false);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);

    MTL::Texture* texture = device->newTexture(descriptor);
    if (texture == nullptr)
    {
        return nullptr;
    }

    texture->replaceRegion(MTL::Region::Make2D(0, 0, kAtlasSize, kAtlasSize), 0, atlas_pixels.data(),
                           kAtlasSize * sizeof(Pixel));
    return texture;
}

MTL::Texture* CreateColorTarget(MTL::Device* device)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, false);
    descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);
    return device->newTexture(descriptor);
}

void WriteDiagnostics(const std::filesystem::path& output_path,
                      const std::vector<Pixel>& output_pixels,
                      const std::vector<Pixel>& atlas_pixels,
                      const Pixel& corner_pixel,
                      const Pixel& center_pixel,
                      const Pixel& sprite_pixel)
{
    std::filesystem::create_directories(output_path.parent_path());

    const std::filesystem::path atlas_path = output_path.parent_path() / "p4_6a_sprite_atlas.ppm";
    WritePpm(output_path, output_pixels, kWidth, kHeight);
    WritePpm(atlas_path, atlas_pixels, kAtlasSize, kAtlasSize);

    std::ofstream log(output_path.parent_path() / "p4_6a_render.log", std::ios::binary);
    log << "P4.6.6 手写 2D 样本 A\n";
    log << "atlas=" << kAtlasSize << "x" << kAtlasSize << ", tile=" << kTileSize << "x" << kTileSize << "\n";
    log << "background tile: uv=(0.00,0.00)-(0.50,0.50)\n";
    log << "sprite tile: uv=(0.50,0.50)-(1.00,1.00), alpha blend=sourceAlpha/oneMinusSourceAlpha\n";
    log << "corner pixel RGBA=(" << static_cast<int>(corner_pixel.r) << ", "
        << static_cast<int>(corner_pixel.g) << ", " << static_cast<int>(corner_pixel.b) << ", "
        << static_cast<int>(corner_pixel.a) << ")\n";
    log << "center pixel RGBA=(" << static_cast<int>(center_pixel.r) << ", "
        << static_cast<int>(center_pixel.g) << ", " << static_cast<int>(center_pixel.b) << ", "
        << static_cast<int>(center_pixel.a) << ")\n";
    log << "sprite sample RGBA=(" << static_cast<int>(sprite_pixel.r) << ", "
        << static_cast<int>(sprite_pixel.g) << ", " << static_cast<int>(sprite_pixel.b) << ", "
        << static_cast<int>(sprite_pixel.a) << ")\n";
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

    MTL::CommandQueue* command_queue = device->newCommandQueue();
    if (command_queue == nullptr)
    {
        std::cerr << "无法创建 MTLCommandQueue。\n";
        pool->drain();
        return 1;
    }

    const char* shader_source = R"MSL(
#include <metal_stdlib>
using namespace metal;

struct VertexOut {
    float4 position [[position]];
    float2 uv;
    float4 tint;
};

vertex VertexOut vertexMain(uint vertexID [[vertex_id]],
                            uint instanceID [[instance_id]])
{
    constexpr float2 positions[6] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2(-1.0,  1.0),
        float2(-1.0,  1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0),
    };
    constexpr float2 uvs[6] = {
        float2(0.0, 1.0),
        float2(1.0, 1.0),
        float2(0.0, 0.0),
        float2(0.0, 0.0),
        float2(1.0, 1.0),
        float2(1.0, 0.0),
    };
    constexpr float2 scales[2] = {
        float2(1.0, 1.0),
        float2(0.58, 0.58),
    };
    constexpr float2 translates[2] = {
        float2(0.0, 0.0),
        float2(0.18, -0.12),
    };
    constexpr float2 uv_scales[2] = {
        float2(0.5, 0.5),
        float2(0.5, 0.5),
    };
    constexpr float2 uv_offsets[2] = {
        float2(0.0, 0.0),
        float2(0.5, 0.5),
    };
    constexpr float4 tints[2] = {
        float4(1.0, 1.0, 1.0, 1.0),
        float4(1.0, 0.95, 1.0, 1.0),
    };

    VertexOut out;
    float2 local = positions[vertexID] * scales[instanceID] + translates[instanceID];
    out.position = float4(local, 0.0, 1.0);
    out.uv = uvs[vertexID] * uv_scales[instanceID] + uv_offsets[instanceID];
    out.tint = tints[instanceID];
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]],
                             texture2d<float> atlas [[texture(0)]],
                             sampler atlasSampler [[sampler(0)]])
{
    return atlas.sample(atlasSampler, in.uv) * in.tint;
}
)MSL";

    NS::Error* error = nullptr;
    NS::String* source = NS::String::string(shader_source, NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(source, nullptr, &error);
    if (library == nullptr)
    {
        std::cerr << "手写 MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::Function* vertex_function = library->newFunction(MTLSTR("vertexMain"));
    MTL::Function* fragment_function = library->newFunction(MTLSTR("fragmentMain"));
    if (vertex_function == nullptr || fragment_function == nullptr)
    {
        std::cerr << "无法获取着色器入口函数。\n";
        pool->drain();
        return 1;
    }

    MTL::RenderPipelineDescriptor* pipeline_descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pipeline_descriptor->setVertexFunction(vertex_function);
    pipeline_descriptor->setFragmentFunction(fragment_function);
    pipeline_descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    pipeline_descriptor->colorAttachments()->object(0)->setBlendingEnabled(true);
    pipeline_descriptor->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
    pipeline_descriptor->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorSourceAlpha);
    pipeline_descriptor->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);
    pipeline_descriptor->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
    pipeline_descriptor->colorAttachments()->object(0)->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    pipeline_descriptor->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

    MTL::RenderPipelineState* pipeline_state = device->newRenderPipelineState(pipeline_descriptor, &error);
    if (pipeline_state == nullptr)
    {
        std::cerr << "无法创建 RenderPipelineState: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    const std::vector<Pixel> atlas_pixels = BuildAtlas();
    MTL::Texture* atlas_texture = CreateAtlasTexture(device, atlas_pixels);
    if (atlas_texture == nullptr)
    {
        std::cerr << "无法创建 sprite atlas 纹理。\n";
        pool->drain();
        return 1;
    }

    MTL::SamplerDescriptor* sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterNearest);
    sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterNearest);
    sampler_descriptor->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
    sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeClampToEdge);

    MTL::SamplerState* sampler_state = device->newSamplerState(sampler_descriptor);
    if (sampler_state == nullptr)
    {
        std::cerr << "无法创建 sampler。\n";
        pool->drain();
        return 1;
    }

    MTL::Texture* color_texture = CreateColorTarget(device);
    if (color_texture == nullptr)
    {
        std::cerr << "无法创建颜色目标纹理。\n";
        pool->drain();
        return 1;
    }

    MTL::RenderPassDescriptor* pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
    pass_descriptor->colorAttachments()->object(0)->setTexture(color_texture);
    pass_descriptor->colorAttachments()->object(0)->setLoadAction(MTL::LoadActionClear);
    pass_descriptor->colorAttachments()->object(0)->setStoreAction(MTL::StoreActionStore);
    pass_descriptor->colorAttachments()->object(0)->setClearColor(MTL::ClearColor(0.10, 0.12, 0.16, 1.0));

    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass_descriptor);
    encoder->setRenderPipelineState(pipeline_state);
    encoder->setFragmentTexture(atlas_texture, 0);
    encoder->setFragmentSamplerState(sampler_state, 0);

    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6), NS::UInteger(2));
    encoder->endEncoding();
    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    std::vector<Pixel> output_pixels(kWidth * kHeight);
    color_texture->getBytes(
        output_pixels.data(),
        static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
        MTL::Region::Make2D(0, 0, kWidth, kHeight),
        0);

    const std::filesystem::path out_dir = "out";
    std::filesystem::create_directories(out_dir);
    const std::filesystem::path output_path = out_dir / "p4_6a_sprite_quad.ppm";

    const Pixel corner_pixel = output_pixels.front();
    const Pixel center_pixel = output_pixels[(kHeight / 2) * kWidth + (kWidth / 2)];
    const Pixel sprite_pixel = output_pixels[(kHeight / 2) * kWidth + (kWidth / 2 + 36)];
    WriteDiagnostics(output_path, output_pixels, atlas_pixels, corner_pixel, center_pixel, sprite_pixel);

    std::cout << "P4.6.6 手写 2D 样本完成: " << output_path << "\n";
    std::cout << "atlas: out/p4_6a_sprite_atlas.ppm\n";
    std::cout << "corner pixel RGBA=(" << static_cast<int>(corner_pixel.r) << ", "
              << static_cast<int>(corner_pixel.g) << ", " << static_cast<int>(corner_pixel.b) << ", "
              << static_cast<int>(corner_pixel.a) << ")\n";
    std::cout << "center pixel RGBA=(" << static_cast<int>(center_pixel.r) << ", "
              << static_cast<int>(center_pixel.g) << ", " << static_cast<int>(center_pixel.b) << ", "
              << static_cast<int>(center_pixel.a) << ")\n";
    std::cout << "sprite pixel RGBA=(" << static_cast<int>(sprite_pixel.r) << ", "
              << static_cast<int>(sprite_pixel.g) << ", " << static_cast<int>(sprite_pixel.b) << ", "
              << static_cast<int>(sprite_pixel.a) << ")\n";
    std::cout << "atlas tile: background=(0,0), sprite=(1,1)\n";
    std::cout << "blend: sourceAlpha / oneMinusSourceAlpha\n";

    pool->drain();
    return 0;
}
