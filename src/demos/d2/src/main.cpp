#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t kWidth = 256;
constexpr std::uint32_t kHeight = 256;

struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

struct Vertex
{
    float position[2];
    float uv[2];
};

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

std::string ErrorToString(NS::Error* error)
{
    if (error == nullptr)
    {
        return "未知错误";
    }

    NS::String* description = error->localizedDescription();
    return description != nullptr ? description->utf8String() : "未知错误";
}

MTL::Library* LoadMetallib(MTL::Device* device, const std::filesystem::path& path)
{
    NS::Error* error = nullptr;
    NS::String* library_path = NS::String::string(path.c_str(), NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(library_path, &error);

    if (library == nullptr)
    {
        std::cerr << "无法加载 metallib " << path << ": " << ErrorToString(error) << "\n";
    }

    return library;
}
}

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

    MTL::Library* vertex_library = LoadMetallib(device, "build/quad_vertex.metallib");
    MTL::Library* fragment_library = LoadMetallib(device, "build/quad_fragment.metallib");
    if (vertex_library == nullptr || fragment_library == nullptr)
    {
        pool->drain();
        return 1;
    }

    MTL::Function* vertex_function = vertex_library->newFunction(MTLSTR("vertexMain"));
    MTL::Function* fragment_function = fragment_library->newFunction(MTLSTR("fragmentMain"));
    if (vertex_function == nullptr || fragment_function == nullptr)
    {
        std::cerr << "无法从 metallib 中取出着色器入口。\n";
        pool->drain();
        return 1;
    }

    NS::Error* error = nullptr;
    MTL::RenderPipelineDescriptor* pipeline_descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    pipeline_descriptor->setVertexFunction(vertex_function);
    pipeline_descriptor->setFragmentFunction(fragment_function);
    pipeline_descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);

    MTL::RenderPipelineState* pipeline_state = device->newRenderPipelineState(pipeline_descriptor, &error);
    if (pipeline_state == nullptr)
    {
        std::cerr << "无法创建 RenderPipelineState: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    const Vertex quad_vertices[6] = {
        {{-0.8f,  0.8f}, {0.0f, 0.0f}},
        {{-0.8f, -0.8f}, {0.0f, 1.0f}},
        {{ 0.8f, -0.8f}, {1.0f, 1.0f}},
        {{-0.8f,  0.8f}, {0.0f, 0.0f}},
        {{ 0.8f, -0.8f}, {1.0f, 1.0f}},
        {{ 0.8f,  0.8f}, {1.0f, 0.0f}},
    };

    MTL::Buffer* vertex_buffer = device->newBuffer(quad_vertices, sizeof(quad_vertices), MTL::ResourceStorageModeShared);
    if (vertex_buffer == nullptr)
    {
        std::cerr << "无法创建顶点缓冲区。\n";
        pool->drain();
        return 1;
    }

    const Pixel texture_pixels[4] = {
        {  0,   0, 255, 255},
        {  0, 255,   0, 255},
        {255,   0,   0, 255},
        {255, 255, 255, 255},
    };

    MTL::TextureDescriptor* sample_texture_descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, 2, 2, false);
    sample_texture_descriptor->setUsage(MTL::TextureUsageShaderRead);
    sample_texture_descriptor->setStorageMode(MTL::StorageModeShared);

    MTL::Texture* sample_texture = device->newTexture(sample_texture_descriptor);
    sample_texture->replaceRegion(MTL::Region::Make2D(0, 0, 2, 2), 0, texture_pixels, 2 * sizeof(Pixel));

    MTL::SamplerDescriptor* sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
    sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);
    sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeClampToEdge);

    MTL::SamplerState* sampler_state = device->newSamplerState(sampler_descriptor);
    if (sampler_state == nullptr)
    {
        std::cerr << "无法创建采样器状态。\n";
        pool->drain();
        return 1;
    }

    MTL::TextureDescriptor* color_texture_descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, false);
    color_texture_descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    color_texture_descriptor->setStorageMode(MTL::StorageModeShared);

    MTL::Texture* color_texture = device->newTexture(color_texture_descriptor);
    if (color_texture == nullptr)
    {
        std::cerr << "无法创建离屏颜色纹理。\n";
        pool->drain();
        return 1;
    }

    MTL::RenderPassDescriptor* pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* color_attachment = pass_descriptor->colorAttachments()->object(0);
    color_attachment->setTexture(color_texture);
    color_attachment->setLoadAction(MTL::LoadActionClear);
    color_attachment->setStoreAction(MTL::StoreActionStore);
    color_attachment->setClearColor(MTL::ClearColor(0.08, 0.10, 0.14, 1.0));

    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass_descriptor);
    encoder->setRenderPipelineState(pipeline_state);
    encoder->setVertexBuffer(vertex_buffer, 0, 0);
    encoder->setFragmentTexture(sample_texture, 0);
    encoder->setFragmentSamplerState(sampler_state, 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(6));
    encoder->endEncoding();
    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    std::vector<Pixel> pixels(kWidth * kHeight);
    color_texture->getBytes(
        pixels.data(),
        static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
        MTL::Region::Make2D(0, 0, kWidth, kHeight),
        0);

    std::filesystem::create_directories("out");
    const std::filesystem::path output_path = std::filesystem::path("out") / "textured_quad.ppm";
    if (!WritePpm(output_path, pixels))
    {
        std::cerr << "无法写出离屏纹理采样结果: " << output_path << "\n";
        pool->drain();
        return 1;
    }

    std::cout << "D2 离屏渲染完成: " << output_path << "\n";
    std::cout << "设备: " << device->name()->utf8String() << "\n";
    std::cout << "顶点 metallib: build/quad_vertex.metallib\n";
    std::cout << "片段 metallib: build/quad_fragment.metallib\n";

    pool->drain();
    return 0;
}
