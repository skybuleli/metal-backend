#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include "scene_common.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace
{

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
              const std::vector<d4::Pixel>& pixels,
              std::uint32_t width,
              std::uint32_t height)
{
    std::ofstream output(output_path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P6\n" << width << " " << height << "\n255\n";
    for (const d4::Pixel& pixel : pixels)
    {
        output.put(static_cast<char>(pixel.r));
        output.put(static_cast<char>(pixel.g));
        output.put(static_cast<char>(pixel.b));
    }

    return true;
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
    NS::String* source = NS::String::string(d4::kShaderSource, NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(source, nullptr, &error);
    if (library == nullptr)
    {
        std::cerr << "MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "MSL 着色器编译通过\n";

    MTL::Function* vertex_function = library->newFunction(MTLSTR("vertexMain"));
    MTL::Function* fragment_function = library->newFunction(MTLSTR("fragmentMain"));
    if (vertex_function == nullptr || fragment_function == nullptr)
    {
        std::cerr << "无法从 Library 获取着色器入口函数。\n";
        pool->drain();
        return 1;
    }

    const std::vector<float> positions = d4::BuildPositionStream();
    const std::vector<float> normals = d4::BuildNormalStream();
    const std::uint32_t vertex_bytes = d4::kNumCubeVertices * sizeof(float) * 3;

    MTL::Buffer* vertex_position_buffer =
        device->newBuffer(positions.data(), vertex_bytes, MTL::ResourceStorageModeShared);
    if (vertex_position_buffer == nullptr)
    {
        std::cerr << "无法创建顶点位置缓冲区。\n";
        pool->drain();
        return 1;
    }

    MTL::Buffer* vertex_normal_buffer =
        device->newBuffer(normals.data(), vertex_bytes, MTL::ResourceStorageModeShared);
    if (vertex_normal_buffer == nullptr)
    {
        std::cerr << "无法创建顶点法线缓冲区。\n";
        pool->drain();
        return 1;
    }

    const d4::SceneMatrices matrices = d4::BuildSceneMatrices(
        static_cast<float>(d4::kOffscreenWidth) / static_cast<float>(d4::kOffscreenHeight),
        0.3f,
        0.6f);
    d4::UniformData uniform_data = d4::BuildUniformData(
        static_cast<float>(d4::kOffscreenWidth) / static_cast<float>(d4::kOffscreenHeight),
        0.3f,
        0.6f);

    std::cout << "MVP 矩阵:\n";
    for (int row = 0; row < 4; ++row)
    {
        std::cout << "  [";
        for (int col = 0; col < 4; ++col)
        {
            std::cout << matrices.mvp.m[col * 4 + row] << (col < 3 ? ", " : "");
        }
        std::cout << "]\n";
    }

    const float test_vertex[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    float clip_position[4] = {};
    for (int row = 0; row < 4; ++row)
    {
        for (int col = 0; col < 4; ++col)
        {
            clip_position[row] += matrices.mvp.m[col * 4 + row] * test_vertex[col];
        }
    }
    std::cout << "测试顶点 (0.5,0.5,0.5) → clip ("
              << clip_position[0] << ", " << clip_position[1] << ", "
              << clip_position[2] << ", " << clip_position[3] << ")\n";
    if (clip_position[3] != 0.0f)
    {
        std::cout << "  归一化 NDC: ("
                  << clip_position[0] / clip_position[3] << ", "
                  << clip_position[1] / clip_position[3] << ", "
                  << clip_position[2] / clip_position[3] << ")\n";
    }

    MTL::Buffer* uniform_buffer =
        device->newBuffer(&uniform_data, sizeof(d4::UniformData), MTL::ResourceStorageModeShared);
    if (uniform_buffer == nullptr)
    {
        std::cerr << "无法创建 Uniform Buffer。\n";
        pool->drain();
        return 1;
    }
    std::cout << "Uniform Buffer 已创建, 大小=" << sizeof(d4::UniformData) << " 字节\n";
    std::cout << "光源位置: (" << d4::kLightPosition[0] << ", "
              << d4::kLightPosition[1] << ", " << d4::kLightPosition[2] << ")\n";
    std::cout << "相机位置: (" << d4::kCameraPosition[0] << ", "
              << d4::kCameraPosition[1] << ", " << d4::kCameraPosition[2] << ")\n";

    MTL::TextureDescriptor* depth_texture_descriptor = MTL::TextureDescriptor::alloc()->init();
    depth_texture_descriptor->setTextureType(MTL::TextureType2D);
    depth_texture_descriptor->setPixelFormat(MTL::PixelFormatDepth32Float);
    depth_texture_descriptor->setWidth(d4::kOffscreenWidth);
    depth_texture_descriptor->setHeight(d4::kOffscreenHeight);
    depth_texture_descriptor->setUsage(MTL::TextureUsageRenderTarget);
    depth_texture_descriptor->setStorageMode(MTL::StorageModePrivate);
    MTL::Texture* depth_texture = device->newTexture(depth_texture_descriptor);
    if (depth_texture == nullptr)
    {
        std::cerr << "无法创建深度纹理。\n";
        pool->drain();
        return 1;
    }
    std::cout << "深度纹理已创建: " << d4::kOffscreenWidth << "x" << d4::kOffscreenHeight
              << ", format=Depth32Float\n";

    MTL::DepthStencilDescriptor* depth_stencil_descriptor =
        MTL::DepthStencilDescriptor::alloc()->init();
    depth_stencil_descriptor->setDepthCompareFunction(MTL::CompareFunctionLess);
    depth_stencil_descriptor->setDepthWriteEnabled(true);
    MTL::DepthStencilState* depth_stencil_state =
        device->newDepthStencilState(depth_stencil_descriptor);
    if (depth_stencil_state == nullptr)
    {
        std::cerr << "无法创建 DepthStencilState。\n";
        pool->drain();
        return 1;
    }
    std::cout << "DepthStencilState 已创建: CompareLess, WriteEnabled\n";

    MTL::TextureDescriptor* color_texture_descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(
            MTL::PixelFormatBGRA8Unorm, d4::kOffscreenWidth, d4::kOffscreenHeight, false);
    color_texture_descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    color_texture_descriptor->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* color_texture = device->newTexture(color_texture_descriptor);
    if (color_texture == nullptr)
    {
        std::cerr << "无法创建颜色纹理。\n";
        pool->drain();
        return 1;
    }

    MTL::RenderPipelineDescriptor* pipeline_descriptor =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipeline_descriptor->setVertexFunction(vertex_function);
    pipeline_descriptor->setFragmentFunction(fragment_function);
    pipeline_descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    pipeline_descriptor->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState* pipeline_state =
        device->newRenderPipelineState(pipeline_descriptor, &error);
    if (pipeline_state == nullptr)
    {
        std::cerr << "无法创建 RenderPipelineState: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "RenderPipelineState 已创建 (depth format=Depth32Float)\n";

    MTL::RenderPassDescriptor* pass_descriptor = MTL::RenderPassDescriptor::alloc()->init();
    MTL::RenderPassColorAttachmentDescriptor* color_attachment =
        pass_descriptor->colorAttachments()->object(0);
    color_attachment->setTexture(color_texture);
    color_attachment->setLoadAction(MTL::LoadActionClear);
    color_attachment->setStoreAction(MTL::StoreActionStore);
    color_attachment->setClearColor(
        MTL::ClearColor(d4::kClearColorR, d4::kClearColorG, d4::kClearColorB, 1.0));

    MTL::RenderPassDepthAttachmentDescriptor* depth_attachment = pass_descriptor->depthAttachment();
    depth_attachment->setTexture(depth_texture);
    depth_attachment->setLoadAction(MTL::LoadActionClear);
    depth_attachment->setStoreAction(MTL::StoreActionDontCare);
    depth_attachment->setClearDepth(1.0);

    MTL::CommandBuffer* command_buffer = command_queue->commandBuffer();
    MTL::RenderCommandEncoder* encoder = command_buffer->renderCommandEncoder(pass_descriptor);
    encoder->setRenderPipelineState(pipeline_state);
    encoder->setDepthStencilState(depth_stencil_state);
    encoder->setCullMode(MTL::CullModeBack);
    encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);
    encoder->setVertexBuffer(vertex_position_buffer, 0, 0);
    encoder->setVertexBuffer(vertex_normal_buffer, 0, 1);
    encoder->setVertexBuffer(uniform_buffer, 0, 2);
    encoder->setFragmentBuffer(uniform_buffer, 0, 0);
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle, NS::UInteger(0), NS::UInteger(d4::kNumCubeVertices));
    encoder->endEncoding();
    command_buffer->commit();
    command_buffer->waitUntilCompleted();

    std::cout << "绘制完成: " << d4::kNumCubeVertices << " 顶点\n";

    std::vector<d4::Pixel> pixels(d4::kOffscreenWidth * d4::kOffscreenHeight);
    color_texture->getBytes(
        pixels.data(),
        static_cast<NS::UInteger>(d4::kOffscreenWidth * sizeof(d4::Pixel)),
        MTL::Region::Make2D(0, 0, d4::kOffscreenWidth, d4::kOffscreenHeight),
        0);

    std::filesystem::create_directories("out");
    const std::filesystem::path output_path = std::filesystem::path("out") / "basic_lighting.ppm";
    if (!WritePpm(output_path, pixels, d4::kOffscreenWidth, d4::kOffscreenHeight))
    {
        std::cerr << "无法写出渲染结果: " << output_path << "\n";
        pool->drain();
        return 1;
    }

    std::cout << "D4 离屏渲染完成: " << output_path << "\n";

    pool->drain();
    return 0;
}
