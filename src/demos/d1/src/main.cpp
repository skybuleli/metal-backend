#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <array>
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

    const char* shader_source = R"(
        #include <metal_stdlib>
        using namespace metal;

        struct VertexOut
        {
            float4 position [[position]];
            float3 color;
        };

        vertex VertexOut vertexMain(uint vertexId [[vertex_id]])
        {
            constexpr float2 positions[3] = {
                float2(0.0, 0.8),
                float2(-0.8, -0.8),
                float2(0.8, -0.8)
            };

            constexpr float3 colors[3] = {
                float3(1.0, 0.2, 0.2),
                float3(0.2, 1.0, 0.2),
                float3(0.2, 0.4, 1.0)
            };

            VertexOut output;
            output.position = float4(positions[vertexId], 0.0, 1.0);
            output.color = colors[vertexId];
            return output;
        }

        fragment float4 fragmentMain(VertexOut input [[stage_in]])
        {
            return float4(input.color, 1.0);
        }
    )";

    NS::Error* error = nullptr;
    NS::String* source = NS::String::string(shader_source, NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(source, nullptr, &error);
    if (library == nullptr)
    {
        std::cerr << "无法编译内嵌 MSL: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::Function* vertex_function = library->newFunction(MTLSTR("vertexMain"));
    MTL::Function* fragment_function = library->newFunction(MTLSTR("fragmentMain"));
    if (vertex_function == nullptr || fragment_function == nullptr)
    {
        std::cerr << "无法从 Library 中取出着色器入口。\n";
        pool->drain();
        return 1;
    }

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

    MTL::TextureDescriptor* texture_descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, false);
    texture_descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    texture_descriptor->setStorageMode(MTL::StorageModeShared);

    MTL::Texture* color_texture = device->newTexture(texture_descriptor);
    if (color_texture == nullptr)
    {
        std::cerr << "无法创建离屏纹理。\n";
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
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(3));
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
    const std::filesystem::path output_path = std::filesystem::path("out") / "triangle.ppm";
    if (!WritePpm(output_path, pixels))
    {
        std::cerr << "无法写出离屏渲染结果: " << output_path << "\n";
        pool->drain();
        return 1;
    }

    std::cout << "D1 离屏渲染完成: " << output_path << "\n";
    std::cout << "设备: " << device->name()->utf8String() << "\n";

    pool->drain();
    return 0;
}
