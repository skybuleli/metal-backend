#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr std::uint32_t kWindowWidth = 1024;
constexpr std::uint32_t kWindowHeight = 1024;
constexpr std::size_t kPreviewCount = 4;

struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

struct Image
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::vector<Pixel> pixels;
};

struct Vertex
{
    float position[2];
    float uv[2];
};

bool ReadPpm(const std::filesystem::path& path, Image& image)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
    {
        return false;
    }

    std::string magic;
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::uint32_t max_value = 0;
    input >> magic >> width >> height >> max_value;
    input.get();

    if (magic != "P6" || max_value != 255)
    {
        return false;
    }

    std::vector<std::uint8_t> rgb(width * height * 3);
    input.read(reinterpret_cast<char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
    if (!input)
    {
        return false;
    }

    image.width = width;
    image.height = height;
    image.pixels.resize(width * height);
    for (std::size_t index = 0; index < image.pixels.size(); ++index)
    {
        image.pixels[index] = Pixel{
            rgb[index * 3 + 2],
            rgb[index * 3 + 1],
            rgb[index * 3 + 0],
            255};
    }

    return true;
}

bool WritePpm(const std::filesystem::path& path,
              const std::vector<Pixel>& pixels,
              std::uint32_t width,
              std::uint32_t height)
{
    std::ofstream output(path, std::ios::binary);
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

void BlitScaled(const Image& source,
                std::vector<Pixel>& destination,
                std::uint32_t destination_width,
                std::uint32_t origin_x,
                std::uint32_t origin_y,
                std::uint32_t target_width,
                std::uint32_t target_height)
{
    for (std::uint32_t y = 0; y < target_height; ++y)
    {
        std::uint32_t source_y = static_cast<std::uint32_t>(
            (static_cast<double>(y) / static_cast<double>(target_height)) * static_cast<double>(source.height));
        source_y = std::min(source_y, source.height - 1);

        for (std::uint32_t x = 0; x < target_width; ++x)
        {
            std::uint32_t source_x = static_cast<std::uint32_t>(
                (static_cast<double>(x) / static_cast<double>(target_width)) * static_cast<double>(source.width));
            source_x = std::min(source_x, source.width - 1);

            destination[(origin_y + y) * destination_width + origin_x + x] =
                source.pixels[source_y * source.width + source_x];
        }
    }
}

std::vector<Pixel> BuildCompositePreview(const std::array<Image, kPreviewCount>& images)
{
    std::vector<Pixel> composite(kWindowWidth * kWindowHeight, Pixel{31, 20, 15, 255});
    const std::array<std::pair<std::uint32_t, std::uint32_t>, kPreviewCount> origins = {{
        {0, 0},
        {kWindowWidth / 2, 0},
        {0, kWindowHeight / 2},
        {kWindowWidth / 2, kWindowHeight / 2},
    }};

    for (std::size_t index = 0; index < images.size(); ++index)
    {
        BlitScaled(images[index],
                   composite,
                   kWindowWidth,
                   origins[index].first,
                   origins[index].second,
                   kWindowWidth / 2,
                   kWindowHeight / 2);
    }

    return composite;
}

std::string NsStringToUtf8(NSString* text)
{
    if (text == nil)
    {
        return "未知错误";
    }

    return std::string([text UTF8String]);
}

id<MTLTexture> CreateTextureFromImage(id<MTLDevice> device, const Image& image)
{
    MTLTextureDescriptor* descriptor =
        [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                          width:image.width
                                                         height:image.height
                                                      mipmapped:NO];
    descriptor.usage = MTLTextureUsageShaderRead;
    descriptor.storageMode = MTLStorageModeShared;

    id<MTLTexture> texture = [device newTextureWithDescriptor:descriptor];
    if (texture == nil)
    {
        return nil;
    }

    const MTLRegion region = MTLRegionMake2D(0, 0, image.width, image.height);
    [texture replaceRegion:region
               mipmapLevel:0
                 withBytes:image.pixels.data()
               bytesPerRow:image.width * sizeof(Pixel)];
    return texture;
}

id<MTLBuffer> CreateQuadBuffer(id<MTLDevice> device, float left, float right, float top, float bottom)
{
    const Vertex vertices[6] = {
        {{left,  top},    {0.0f, 0.0f}},
        {{left,  bottom}, {0.0f, 1.0f}},
        {{right, bottom}, {1.0f, 1.0f}},
        {{left,  top},    {0.0f, 0.0f}},
        {{right, bottom}, {1.0f, 1.0f}},
        {{right, top},    {1.0f, 0.0f}},
    };

    return [device newBufferWithBytes:vertices
                               length:sizeof(vertices)
                              options:MTLResourceStorageModeShared];
}

bool RenderPreviewWindow(const std::array<Image, kPreviewCount>& images,
                         double auto_close_seconds,
                         const std::filesystem::path& export_ppm_path)
{
    @autoreleasepool
    {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil)
        {
            std::cerr << "无法创建预览窗口 MTLDevice。\n";
            return false;
        }

        id<MTLCommandQueue> command_queue = [device newCommandQueue];
        if (command_queue == nil)
        {
            std::cerr << "无法创建预览窗口 MTLCommandQueue。\n";
            return false;
        }

        NSString* shader_source = @R"(
            #include <metal_stdlib>
            using namespace metal;

            struct VertexIn
            {
                float2 position [[attribute(0)]];
                float2 uv [[attribute(1)]];
            };

            struct VertexOut
            {
                float4 position [[position]];
                float2 uv;
            };

            vertex VertexOut vertexMain(VertexIn input [[stage_in]])
            {
                VertexOut output;
                output.position = float4(input.position, 0.0, 1.0);
                output.uv = input.uv;
                return output;
            }

            fragment float4 fragmentMain(VertexOut input [[stage_in]],
                                         texture2d<float> colorTexture [[texture(0)]],
                                         sampler colorSampler [[sampler(0)]])
            {
                return colorTexture.sample(colorSampler, input.uv);
            }
        )";

        NSError* error = nil;
        id<MTLLibrary> library = [device newLibraryWithSource:shader_source options:nil error:&error];
        if (library == nil)
        {
            std::cerr << "预览窗口 MSL 编译失败: " << NsStringToUtf8(error.localizedDescription) << "\n";
            return false;
        }

        id<MTLFunction> vertex_function = [library newFunctionWithName:@"vertexMain"];
        id<MTLFunction> fragment_function = [library newFunctionWithName:@"fragmentMain"];
        if (vertex_function == nil || fragment_function == nil)
        {
            std::cerr << "无法创建预览窗口着色器函数。\n";
            return false;
        }

        MTLRenderPipelineDescriptor* pipeline_descriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipeline_descriptor.vertexFunction = vertex_function;
        pipeline_descriptor.fragmentFunction = fragment_function;
        pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

        MTLVertexDescriptor* vertex_descriptor = [[MTLVertexDescriptor alloc] init];
        vertex_descriptor.attributes[0].format = MTLVertexFormatFloat2;
        vertex_descriptor.attributes[0].offset = 0;
        vertex_descriptor.attributes[0].bufferIndex = 0;
        vertex_descriptor.attributes[1].format = MTLVertexFormatFloat2;
        vertex_descriptor.attributes[1].offset = sizeof(float) * 2;
        vertex_descriptor.attributes[1].bufferIndex = 0;
        vertex_descriptor.layouts[0].stride = sizeof(Vertex);
        vertex_descriptor.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
        pipeline_descriptor.vertexDescriptor = vertex_descriptor;

        id<MTLRenderPipelineState> pipeline_state =
            [device newRenderPipelineStateWithDescriptor:pipeline_descriptor error:&error];
        if (pipeline_state == nil)
        {
            std::cerr << "无法创建预览窗口 RenderPipelineState: " << NsStringToUtf8(error.localizedDescription)
                      << "\n";
            return false;
        }

        std::array<id<MTLTexture>, kPreviewCount> textures = {};
        for (std::size_t index = 0; index < images.size(); ++index)
        {
            textures[index] = CreateTextureFromImage(device, images[index]);
            if (textures[index] == nil)
            {
                std::cerr << "无法为预览窗口创建纹理: #" << index << "\n";
                return false;
            }
        }

        const float gap = 0.04f;
        std::array<id<MTLBuffer>, kPreviewCount> quad_buffers = {
            CreateQuadBuffer(device, -1.0f, -gap, 1.0f, gap),
            CreateQuadBuffer(device, gap, 1.0f, 1.0f, gap),
            CreateQuadBuffer(device, -1.0f, -gap, -gap, -1.0f),
            CreateQuadBuffer(device, gap, 1.0f, -gap, -1.0f),
        };

        for (std::size_t index = 0; index < quad_buffers.size(); ++index)
        {
            if (quad_buffers[index] == nil)
            {
                std::cerr << "无法创建预览窗口顶点缓冲: #" << index << "\n";
                return false;
            }
        }

        MTLSamplerDescriptor* sampler_descriptor = [[MTLSamplerDescriptor alloc] init];
        sampler_descriptor.minFilter = MTLSamplerMinMagFilterLinear;
        sampler_descriptor.magFilter = MTLSamplerMinMagFilterLinear;
        sampler_descriptor.sAddressMode = MTLSamplerAddressModeClampToEdge;
        sampler_descriptor.tAddressMode = MTLSamplerAddressModeClampToEdge;
        id<MTLSamplerState> sampler_state = [device newSamplerStateWithDescriptor:sampler_descriptor];
        if (sampler_state == nil)
        {
            std::cerr << "无法创建预览窗口采样器。\n";
            return false;
        }

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        const NSRect frame = NSMakeRect(0.0, 0.0, static_cast<CGFloat>(kWindowWidth), static_cast<CGFloat>(kWindowHeight));
        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                  NSWindowStyleMaskClosable |
                                                                  NSWindowStyleMaskMiniaturizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        [window setTitle:@"P2.6a Demo Preview"];
        [window center];

        NSView* content_view = [[NSView alloc] initWithFrame:frame];
        [content_view setWantsLayer:YES];
        [window setContentView:content_view];

        CAMetalLayer* metal_layer = [CAMetalLayer layer];
        metal_layer.device = device;
        metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metal_layer.framebufferOnly = NO;
        metal_layer.drawableSize = CGSizeMake(kWindowWidth, kWindowHeight);
        metal_layer.frame = content_view.bounds;
        metal_layer.contentsScale = window.backingScaleFactor;
        metal_layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        [content_view setLayer:metal_layer];

        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        std::cout << "窗口已创建: 1024x1024, 四宫格预览 D1-D4\n";

        if (!export_ppm_path.empty())
        {
            std::filesystem::create_directories(export_ppm_path.parent_path());
            if (!WritePpm(export_ppm_path, BuildCompositePreview(images), kWindowWidth, kWindowHeight))
            {
                std::cerr << "无法导出预览窗口组合 PPM: " << export_ppm_path << "\n";
                return false;
            }
            std::cout << "已导出预览组合图: " << export_ppm_path << "\n";
        }

        const auto start_time = std::chrono::steady_clock::now();
        std::uint32_t presented_frames = 0;

        while (true)
        {
            @autoreleasepool
            {
                for (;;)
                {
                    NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                        untilDate:[NSDate distantPast]
                                                           inMode:NSDefaultRunLoopMode
                                                          dequeue:YES];
                    if (event == nil)
                    {
                        break;
                    }

                    [NSApp sendEvent:event];
                    [NSApp updateWindows];
                }

                if (![window isVisible])
                {
                    break;
                }

                id<CAMetalDrawable> drawable = [metal_layer nextDrawable];
                if (drawable != nil)
                {
                    MTLRenderPassDescriptor* pass_descriptor = [MTLRenderPassDescriptor renderPassDescriptor];
                    pass_descriptor.colorAttachments[0].texture = drawable.texture;
                    pass_descriptor.colorAttachments[0].loadAction = MTLLoadActionClear;
                    pass_descriptor.colorAttachments[0].storeAction = MTLStoreActionStore;
                    pass_descriptor.colorAttachments[0].clearColor = MTLClearColorMake(0.06, 0.08, 0.12, 1.0);

                    id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];
                    id<MTLRenderCommandEncoder> encoder =
                        [command_buffer renderCommandEncoderWithDescriptor:pass_descriptor];
                    [encoder setRenderPipelineState:pipeline_state];
                    [encoder setFragmentSamplerState:sampler_state atIndex:0];

                    for (std::size_t index = 0; index < quad_buffers.size(); ++index)
                    {
                        [encoder setVertexBuffer:quad_buffers[index] offset:0 atIndex:0];
                        [encoder setFragmentTexture:textures[index] atIndex:0];
                        [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:6];
                    }

                    [encoder endEncoding];
                    [command_buffer presentDrawable:drawable];
                    [command_buffer commit];
                    [command_buffer waitUntilCompleted];
                    ++presented_frames;
                }
            }

            if (auto_close_seconds > 0.0)
            {
                const double elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
                if (elapsed >= auto_close_seconds)
                {
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        [window close];
        std::cout << "Present 已调用: " << presented_frames << " 帧\n";
        std::cout << "预览窗口已关闭\n";
        return presented_frames > 0;
    }
}
}

int main(int argc, char** argv)
{
    std::vector<std::string> image_paths;
    std::filesystem::path export_ppm_path;
    double auto_close_seconds = 0.0;

    for (int index = 1; index < argc; ++index)
    {
        const std::string argument = argv[index];
        if (argument == "--auto-close-seconds" && index + 1 < argc)
        {
            auto_close_seconds = std::stod(argv[++index]);
            continue;
        }
        if (argument == "--export-ppm" && index + 1 < argc)
        {
            export_ppm_path = argv[++index];
            continue;
        }

        image_paths.push_back(argument);
    }

    if (image_paths.size() != kPreviewCount)
    {
        std::cerr << "用法: demos_preview [--auto-close-seconds 秒] [--export-ppm 路径] d1.ppm d2.ppm d3.ppm d4.ppm\n";
        return 1;
    }

    std::array<Image, kPreviewCount> images;
    for (std::size_t index = 0; index < images.size(); ++index)
    {
        if (!ReadPpm(image_paths[index], images[index]))
        {
            std::cerr << "无法读取预览图像: " << image_paths[index] << "\n";
            return 1;
        }
    }

    if (!RenderPreviewWindow(images, auto_close_seconds, export_ppm_path))
    {
        return 1;
    }

    return 0;
}
