#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "scene_common.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{

constexpr std::uint32_t kWindowWidth = 960;
constexpr std::uint32_t kWindowHeight = 960;

std::string NsStringToUtf8(NSString* text)
{
    if (text == nil)
    {
        return "未知错误";
    }

    return std::string([text UTF8String]);
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

id<MTLTexture> CreateDepthTexture(id<MTLDevice> device, std::uint32_t width, std::uint32_t height)
{
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatDepth32Float
                                                                                           width:width
                                                                                          height:height
                                                                                       mipmapped:NO];
    descriptor.storageMode = MTLStorageModePrivate;
    descriptor.usage = MTLTextureUsageRenderTarget;
    return [device newTextureWithDescriptor:descriptor];
}

id<MTLTexture> CreateCaptureTexture(id<MTLDevice> device, std::uint32_t width, std::uint32_t height)
{
    MTLTextureDescriptor* descriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:MTLPixelFormatBGRA8Unorm
                                                                                           width:width
                                                                                          height:height
                                                                                       mipmapped:NO];
    descriptor.storageMode = MTLStorageModeShared;
    descriptor.usage = MTLTextureUsageRenderTarget | MTLTextureUsageShaderRead;
    return [device newTextureWithDescriptor:descriptor];
}

void EncodeCubePass(id<MTLRenderCommandEncoder> encoder,
                    id<MTLRenderPipelineState> pipeline_state,
                    id<MTLDepthStencilState> depth_stencil_state,
                    id<MTLBuffer> vertex_position_buffer,
                    id<MTLBuffer> vertex_normal_buffer,
                    id<MTLBuffer> uniform_buffer)
{
    [encoder setRenderPipelineState:pipeline_state];
    [encoder setDepthStencilState:depth_stencil_state];
    [encoder setCullMode:MTLCullModeBack];
    [encoder setFrontFacingWinding:MTLWindingCounterClockwise];
    [encoder setVertexBuffer:vertex_position_buffer offset:0 atIndex:0];
    [encoder setVertexBuffer:vertex_normal_buffer offset:0 atIndex:1];
    [encoder setVertexBuffer:uniform_buffer offset:0 atIndex:2];
    [encoder setFragmentBuffer:uniform_buffer offset:0 atIndex:0];
    [encoder drawPrimitives:MTLPrimitiveTypeTriangle vertexStart:0 vertexCount:d4::kNumCubeVertices];
    [encoder endEncoding];
}

bool ExportTextureToPpm(id<MTLTexture> texture, const std::filesystem::path& output_path)
{
    if (texture == nil)
    {
        return false;
    }

    const std::uint32_t width = static_cast<std::uint32_t>(texture.width);
    const std::uint32_t height = static_cast<std::uint32_t>(texture.height);
    std::vector<d4::Pixel> pixels(width * height);
    [texture getBytes:pixels.data()
          bytesPerRow:width * sizeof(d4::Pixel)
           fromRegion:MTLRegionMake2D(0, 0, width, height)
          mipmapLevel:0];

    std::filesystem::create_directories(output_path.parent_path());
    return WritePpm(output_path, pixels, width, height);
}

bool RunWindow(double auto_close_seconds, const std::filesystem::path& export_ppm_path)
{
    @autoreleasepool
    {
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        if (device == nil)
        {
            std::cerr << "无法创建实时窗口 MTLDevice。\n";
            return false;
        }

        id<MTLCommandQueue> command_queue = [device newCommandQueue];
        if (command_queue == nil)
        {
            std::cerr << "无法创建实时窗口 MTLCommandQueue。\n";
            return false;
        }

        NSError* error = nil;
        NSString* shader_source = [NSString stringWithUTF8String:d4::kShaderSource];
        id<MTLLibrary> library = [device newLibraryWithSource:shader_source options:nil error:&error];
        if (library == nil)
        {
            std::cerr << "窗口版 MSL 编译失败: " << NsStringToUtf8(error.localizedDescription) << "\n";
            return false;
        }

        id<MTLFunction> vertex_function = [library newFunctionWithName:@"vertexMain"];
        id<MTLFunction> fragment_function = [library newFunctionWithName:@"fragmentMain"];
        if (vertex_function == nil || fragment_function == nil)
        {
            std::cerr << "无法创建窗口版着色器函数。\n";
            return false;
        }

        MTLRenderPipelineDescriptor* pipeline_descriptor = [[MTLRenderPipelineDescriptor alloc] init];
        pipeline_descriptor.vertexFunction = vertex_function;
        pipeline_descriptor.fragmentFunction = fragment_function;
        pipeline_descriptor.colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;
        pipeline_descriptor.depthAttachmentPixelFormat = MTLPixelFormatDepth32Float;

        id<MTLRenderPipelineState> pipeline_state =
            [device newRenderPipelineStateWithDescriptor:pipeline_descriptor error:&error];
        if (pipeline_state == nil)
        {
            std::cerr << "无法创建窗口版 RenderPipelineState: "
                      << NsStringToUtf8(error.localizedDescription) << "\n";
            return false;
        }

        MTLDepthStencilDescriptor* depth_stencil_descriptor = [[MTLDepthStencilDescriptor alloc] init];
        depth_stencil_descriptor.depthCompareFunction = MTLCompareFunctionLess;
        depth_stencil_descriptor.depthWriteEnabled = YES;
        id<MTLDepthStencilState> depth_stencil_state =
            [device newDepthStencilStateWithDescriptor:depth_stencil_descriptor];
        if (depth_stencil_state == nil)
        {
            std::cerr << "无法创建窗口版 DepthStencilState。\n";
            return false;
        }

        const std::vector<float> positions = d4::BuildPositionStream();
        const std::vector<float> normals = d4::BuildNormalStream();
        const std::uint32_t vertex_bytes = d4::kNumCubeVertices * sizeof(float) * 3;
        id<MTLBuffer> vertex_position_buffer =
            [device newBufferWithBytes:positions.data() length:vertex_bytes options:MTLResourceStorageModeShared];
        id<MTLBuffer> vertex_normal_buffer =
            [device newBufferWithBytes:normals.data() length:vertex_bytes options:MTLResourceStorageModeShared];
        id<MTLBuffer> uniform_buffer =
            [device newBufferWithLength:sizeof(d4::UniformData) options:MTLResourceStorageModeShared];
        if (vertex_position_buffer == nil || vertex_normal_buffer == nil || uniform_buffer == nil)
        {
            std::cerr << "无法创建窗口版缓冲区。\n";
            return false;
        }

        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        const NSRect frame = NSMakeRect(0.0, 0.0, static_cast<CGFloat>(kWindowWidth), static_cast<CGFloat>(kWindowHeight));
        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                  NSWindowStyleMaskClosable |
                                                                  NSWindowStyleMaskMiniaturizable |
                                                                  NSWindowStyleMaskResizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        [window setTitle:@"D4 Rotating Cube"];
        [window center];

        NSView* content_view = [[NSView alloc] initWithFrame:frame];
        [content_view setWantsLayer:YES];
        [window setContentView:content_view];

        CAMetalLayer* metal_layer = [CAMetalLayer layer];
        metal_layer.device = device;
        metal_layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metal_layer.framebufferOnly = NO;
        metal_layer.frame = content_view.bounds;
        metal_layer.contentsScale = window.backingScaleFactor;
        metal_layer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        metal_layer.drawableSize = CGSizeMake(kWindowWidth * metal_layer.contentsScale,
                                              kWindowHeight * metal_layer.contentsScale);
        [content_view setLayer:metal_layer];

        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        std::uint32_t drawable_width = static_cast<std::uint32_t>(metal_layer.drawableSize.width);
        std::uint32_t drawable_height = static_cast<std::uint32_t>(metal_layer.drawableSize.height);
        id<MTLTexture> depth_texture = CreateDepthTexture(device, drawable_width, drawable_height);
        id<MTLTexture> capture_texture = CreateCaptureTexture(device, drawable_width, drawable_height);
        if (depth_texture == nil || capture_texture == nil)
        {
            std::cerr << "无法创建窗口版渲染纹理。\n";
            return false;
        }

        std::cout << "窗口已创建: " << drawable_width << "x" << drawable_height
                  << ", 自动旋转立方体已启动\n";

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

                const CGSize drawable_size = metal_layer.drawableSize;
                const std::uint32_t new_width = static_cast<std::uint32_t>(drawable_size.width);
                const std::uint32_t new_height = static_cast<std::uint32_t>(drawable_size.height);
                if (new_width != drawable_width || new_height != drawable_height)
                {
                    drawable_width = std::max<std::uint32_t>(new_width, 1);
                    drawable_height = std::max<std::uint32_t>(new_height, 1);
                    depth_texture = CreateDepthTexture(device, drawable_width, drawable_height);
                    capture_texture = CreateCaptureTexture(device, drawable_width, drawable_height);
                    if (depth_texture == nil || capture_texture == nil)
                    {
                        std::cerr << "窗口尺寸变化后无法重建纹理。\n";
                        return false;
                    }
                }

                id<CAMetalDrawable> drawable = [metal_layer nextDrawable];
                if (drawable == nil)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(8));
                    continue;
                }

                const double elapsed_seconds =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
                const float angle_y = static_cast<float>(elapsed_seconds * 1.35);
                const float angle_x = static_cast<float>(0.42 + std::sin(elapsed_seconds * 0.8) * 0.18);
                const float aspect = static_cast<float>(drawable_width) / static_cast<float>(drawable_height);
                const d4::UniformData uniform_data = d4::BuildUniformData(aspect, angle_x, angle_y);
                std::memcpy(uniform_buffer.contents, &uniform_data, sizeof(uniform_data));

                id<MTLCommandBuffer> command_buffer = [command_queue commandBuffer];

                MTLRenderPassDescriptor* window_pass = [MTLRenderPassDescriptor renderPassDescriptor];
                window_pass.colorAttachments[0].texture = drawable.texture;
                window_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
                window_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                window_pass.colorAttachments[0].clearColor =
                    MTLClearColorMake(d4::kClearColorR, d4::kClearColorG, d4::kClearColorB, 1.0);
                window_pass.depthAttachment.texture = depth_texture;
                window_pass.depthAttachment.loadAction = MTLLoadActionClear;
                window_pass.depthAttachment.storeAction = MTLStoreActionDontCare;
                window_pass.depthAttachment.clearDepth = 1.0;

                id<MTLRenderCommandEncoder> window_encoder =
                    [command_buffer renderCommandEncoderWithDescriptor:window_pass];
                EncodeCubePass(window_encoder,
                               pipeline_state,
                               depth_stencil_state,
                               vertex_position_buffer,
                               vertex_normal_buffer,
                               uniform_buffer);

                MTLRenderPassDescriptor* capture_pass = [MTLRenderPassDescriptor renderPassDescriptor];
                capture_pass.colorAttachments[0].texture = capture_texture;
                capture_pass.colorAttachments[0].loadAction = MTLLoadActionClear;
                capture_pass.colorAttachments[0].storeAction = MTLStoreActionStore;
                capture_pass.colorAttachments[0].clearColor =
                    MTLClearColorMake(d4::kClearColorR, d4::kClearColorG, d4::kClearColorB, 1.0);
                capture_pass.depthAttachment.texture = depth_texture;
                capture_pass.depthAttachment.loadAction = MTLLoadActionClear;
                capture_pass.depthAttachment.storeAction = MTLStoreActionDontCare;
                capture_pass.depthAttachment.clearDepth = 1.0;

                id<MTLRenderCommandEncoder> capture_encoder =
                    [command_buffer renderCommandEncoderWithDescriptor:capture_pass];
                EncodeCubePass(capture_encoder,
                               pipeline_state,
                               depth_stencil_state,
                               vertex_position_buffer,
                               vertex_normal_buffer,
                               uniform_buffer);

                [command_buffer presentDrawable:drawable];
                [command_buffer commit];
                [command_buffer waitUntilCompleted];
                ++presented_frames;
            }

            if (auto_close_seconds > 0.0)
            {
                const double elapsed_seconds =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count();
                if (elapsed_seconds >= auto_close_seconds)
                {
                    break;
                }
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }

        if (!export_ppm_path.empty())
        {
            if (!ExportTextureToPpm(capture_texture, export_ppm_path))
            {
                std::cerr << "无法导出窗口版最终帧: " << export_ppm_path << "\n";
                return false;
            }
            std::cout << "已导出窗口版最终帧: " << export_ppm_path << "\n";
        }

        [window close];
        const double total_seconds =
            std::max(0.001, std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time).count());
        std::cout << "Present 已调用: " << presented_frames << " 帧\n";
        std::cout << "平均帧率: " << (static_cast<double>(presented_frames) / total_seconds) << " fps\n";
        std::cout << "实时窗口已关闭\n";
        return presented_frames > 0;
    }
}

} // namespace

int main(int argc, char** argv)
{
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

        std::cerr << "未知参数: " << argument << "\n";
        return 1;
    }

    if (!RunWindow(auto_close_seconds, export_ppm_path))
    {
        return 1;
    }

    return 0;
}
