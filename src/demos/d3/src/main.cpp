#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define IR_RUNTIME_METALCPP
#define IR_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

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

void DumpArguments(const char* stage_name, NS::Array* arguments)
{
    if (arguments == nullptr)
    {
        std::cout << stage_name << " 反射参数: <none>\n";
        return;
    }

    std::cout << stage_name << " 反射参数数量: " << arguments->count() << "\n";
    for (NS::UInteger index = 0; index < arguments->count(); ++index)
    {
        MTL::Argument* argument = arguments->object<MTL::Argument>(index);
        std::cout
            << "  - name=" << argument->name()->utf8String()
            << ", type=" << static_cast<int>(argument->type())
            << ", index=" << argument->index()
            << ", active=" << (argument->active() ? "true" : "false")
            << "\n";
    }
}

MTL::Texture* CreatePrimaryTexture(MTL::Device* device)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, 4, 4, true);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);

    MTL::Texture* texture = device->newTexture(descriptor);
    if (texture == nullptr)
    {
        return nullptr;
    }

    const Pixel level0[16] = {
        {  0,   0, 255, 255}, {  0, 255, 255, 255}, {255, 255,   0, 255}, {255,   0, 255, 255},
        {  0, 255,   0, 255}, {255, 255, 255, 255}, {255,   0,   0, 255}, {  0,   0,   0, 255},
        {255,   0,   0, 255}, {255, 255, 255, 255}, {  0, 255,   0, 255}, {255, 255,   0, 255},
        {255,   0, 255, 255}, {  0, 255, 255, 255}, {255, 128,   0, 255}, {128,   0, 255, 255},
    };
    const Pixel level1[4] = {
        { 80,  40, 220, 255}, { 60, 210, 200, 255},
        {220, 160,  40, 255}, {140, 220, 220, 255},
    };
    const Pixel level2[1] = {
        {160, 160, 160, 255},
    };

    texture->replaceRegion(MTL::Region::Make2D(0, 0, 4, 4), 0, level0, 4 * sizeof(Pixel));
    texture->replaceRegion(MTL::Region::Make2D(0, 0, 2, 2), 1, level1, 2 * sizeof(Pixel));
    texture->replaceRegion(MTL::Region::Make2D(0, 0, 1, 1), 2, level2, sizeof(Pixel));

    return texture;
}

MTL::Texture* CreateOverlayTexture(MTL::Device* device)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, 4, 4, false);
    descriptor->setUsage(MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);

    MTL::Texture* texture = device->newTexture(descriptor);
    if (texture == nullptr)
    {
        return nullptr;
    }

    const Pixel pixels[16] = {
        { 30,  30,  30, 255}, { 30,  30,  30, 255}, { 30,  30,  30, 255}, { 30,  30,  30, 255},
        { 30,  30,  30, 255}, {220,  40,  40,  80}, { 40, 220, 220, 180}, { 30,  30,  30, 255},
        { 30,  30,  30, 255}, {220, 220,  40, 180}, {240, 240, 240, 255}, { 30,  30,  30, 255},
        { 30,  30,  30, 255}, { 30,  30,  30, 255}, { 30,  30,  30, 255}, { 30,  30,  30, 255},
    };

    texture->replaceRegion(MTL::Region::Make2D(0, 0, 4, 4), 0, pixels, 4 * sizeof(Pixel));
    return texture;
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

    MTL::Library* vertex_library = LoadMetallib(device, "build/multi_texture_vertex.metallib");
    MTL::Library* fragment_library = LoadMetallib(device, "build/multi_texture_fragment.metallib");
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

    MTL::RenderPipelineReflection* reflection = nullptr;
    MTL::RenderPipelineState* pipeline_state = device->newRenderPipelineState(
        pipeline_descriptor,
        MTL::PipelineOptionArgumentInfo,
        &reflection,
        &error);
    if (pipeline_state == nullptr)
    {
        std::cerr << "无法创建 RenderPipelineState: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::Texture* primary_texture = CreatePrimaryTexture(device);
    MTL::Texture* overlay_texture = CreateOverlayTexture(device);
    if (primary_texture == nullptr || overlay_texture == nullptr)
    {
        std::cerr << "无法创建多纹理采样所需纹理。\n";
        pool->drain();
        return 1;
    }

    MTL::SamplerDescriptor* primary_sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    primary_sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterLinear);
    primary_sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterLinear);
    primary_sampler_descriptor->setMipFilter(MTL::SamplerMipFilterLinear);
    primary_sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeRepeat);
    primary_sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeRepeat);
    primary_sampler_descriptor->setSupportArgumentBuffers(true);

    MTL::SamplerDescriptor* overlay_sampler_descriptor = MTL::SamplerDescriptor::alloc()->init();
    overlay_sampler_descriptor->setMinFilter(MTL::SamplerMinMagFilterNearest);
    overlay_sampler_descriptor->setMagFilter(MTL::SamplerMinMagFilterNearest);
    overlay_sampler_descriptor->setMipFilter(MTL::SamplerMipFilterNotMipmapped);
    overlay_sampler_descriptor->setSAddressMode(MTL::SamplerAddressModeClampToEdge);
    overlay_sampler_descriptor->setTAddressMode(MTL::SamplerAddressModeClampToEdge);
    overlay_sampler_descriptor->setSupportArgumentBuffers(true);

    MTL::SamplerState* primary_sampler = device->newSamplerState(primary_sampler_descriptor);
    MTL::SamplerState* overlay_sampler = device->newSamplerState(overlay_sampler_descriptor);
    if (primary_sampler == nullptr || overlay_sampler == nullptr)
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
    color_attachment->setClearColor(MTL::ClearColor(0.06, 0.08, 0.12, 1.0));

    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass_descriptor);
    encoder->setRenderPipelineState(pipeline_state);

    // MSC 反射要求顶层参数缓冲按“全部 SRV，随后全部 Sampler”的顺序布局。
    IRDescriptorTableEntry fragment_arguments[4] = {};
    IRDescriptorTableSetTexture(&fragment_arguments[0], primary_texture, 0.0f, 0);
    IRDescriptorTableSetTexture(&fragment_arguments[1], overlay_texture, 0.0f, 0);
    IRDescriptorTableSetSampler(&fragment_arguments[2], primary_sampler, 0.0f);
    IRDescriptorTableSetSampler(&fragment_arguments[3], overlay_sampler, 0.0f);

    MTL::Buffer* fragment_argument_buffer =
        device->newBuffer(fragment_arguments, sizeof(fragment_arguments), MTL::ResourceStorageModeShared);
    if (fragment_argument_buffer == nullptr)
    {
        std::cerr << "无法创建片段参数缓冲区。\n";
        pool->drain();
        return 1;
    }

    encoder->setFragmentBuffer(fragment_argument_buffer, 0, kIRArgumentBufferBindPoint);
    IRRuntimeDrawPrimitives(
        encoder,
        MTL::PrimitiveTypeTriangle,
        static_cast<std::uint64_t>(0),
        static_cast<std::uint64_t>(6));
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
    const std::filesystem::path output_path = std::filesystem::path("out") / "multi_texture.ppm";
    if (!WritePpm(output_path, pixels))
    {
        std::cerr << "无法写出多纹理混合结果: " << output_path << "\n";
        pool->drain();
        return 1;
    }

    std::cout << "D3 离屏渲染完成: " << output_path << "\n";
    std::cout << "设备: " << device->name()->utf8String() << "\n";
    std::cout << "顶点 metallib: build/multi_texture_vertex.metallib\n";
    std::cout << "片段 metallib: build/multi_texture_fragment.metallib\n";
    std::cout << "主采样器: repeat + linear + mipLinear\n";
    std::cout << "叠加采样器: clamp + nearest + notMipmapped\n";
    DumpArguments("vertex", reflection != nullptr ? reflection->vertexArguments() : nullptr);
    DumpArguments("fragment", reflection != nullptr ? reflection->fragmentArguments() : nullptr);

    pool->drain();
    return 0;
}
