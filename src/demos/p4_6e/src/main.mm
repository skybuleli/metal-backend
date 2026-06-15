#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "metal_bridge.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr std::uint32_t kWidth = 960;
constexpr std::uint32_t kHeight = 540;

struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

struct alignas(16) FrameData
{
    float phase;
    float pad[3];
};

static_assert(sizeof(FrameData) == 16, "FrameData 大小必须为 16 字节");

struct ShaderBlob
{
    void* data = nullptr;
    std::uint64_t size = 0;

    ShaderBlob() = default;
    ShaderBlob(ShaderBlob&& other) noexcept
        : data(other.data), size(other.size)
    {
        other.data = nullptr;
        other.size = 0;
    }

    ShaderBlob& operator=(ShaderBlob&& other) noexcept
    {
        if (this != &other)
        {
            if (data != nullptr)
            {
                metal_free_shader_data(data);
            }
            data = other.data;
            size = other.size;
            other.data = nullptr;
            other.size = 0;
        }
        return *this;
    }

    ~ShaderBlob()
    {
        if (data != nullptr)
        {
            metal_free_shader_data(data);
            data = nullptr;
            size = 0;
        }
    }

    ShaderBlob(const ShaderBlob&) = delete;
    ShaderBlob& operator=(const ShaderBlob&) = delete;
};

std::string JsonEscape(const std::string& value)
{
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (char ch : value)
    {
        switch (ch)
        {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
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

std::vector<Pixel> ReadbackToPixels(metal_buffer* buffer)
{
    std::vector<Pixel> pixels(kWidth * kHeight);
    void* mapped = nullptr;
    if (metal_map_buffer(buffer, &mapped) != METAL_RESULT_OK || mapped == nullptr)
    {
        return {};
    }

    std::memcpy(pixels.data(), mapped, pixels.size() * sizeof(Pixel));
    metal_unmap_buffer(buffer);
    return pixels;
}

ShaderBlob CompileStage(metal_shader_compiler* compiler,
                        const char* source,
                        const char* stage,
                        const char* entry_point,
                        const char* profile,
                        const char* label)
{
    ShaderBlob blob;
    metal_shader_compile_result result = metal_compile_shader(
        compiler,
        source,
        "slang",
        stage,
        entry_point,
        profile);

    if (result.result != METAL_RESULT_OK || result.metallib_data == nullptr || result.metallib_size == 0)
    {
        std::cerr << label << " 编译失败: " << result.error_message << "\n";
        return blob;
    }

    blob.data = result.metallib_data;
    blob.size = result.metallib_size;
    result.metallib_data = nullptr;
    result.metallib_size = 0;
    return blob;
}

const char* kVertexSource = R"SLANG(
struct FrameData
{
    float phase;
    float3 pad;
};

struct VSOut
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

[shader("vertex")]
VSOut vs_main(uint vid : SV_VertexID,
              ConstantBuffer<FrameData> frameData : register(b0))
{
    const float2 base_positions[3] = {
        float2(-0.70, -0.55),
        float2(0.00,  0.72),
        float2(0.70, -0.55),
    };
    const float4 base_colors[3] = {
        float4(1.0, 0.24, 0.18, 1.0),
        float4(0.18, 1.0, 0.36, 1.0),
        float4(0.20, 0.42, 1.0, 1.0),
    };

    VSOut output;
    float offset = sin(frameData.phase) * 0.25;
    output.position = float4(base_positions[vid] + float2(offset, 0.0), 0.0, 1.0);
    output.color = base_colors[vid];
    return output;
}
)SLANG";

const char* kFragmentSource = R"SLANG(
struct VSOut
{
    float4 position : SV_Position;
    float4 color : COLOR0;
};

[shader("fragment")]
float4 fs_main(VSOut input) : SV_Target0
{
    return input.color;
}
)SLANG";

bool WriteEvidence(const std::filesystem::path& ppm_path,
                   const std::filesystem::path& json_path,
                   const std::vector<Pixel>& pixels,
                   std::uint32_t presented_frames,
                   std::uint32_t center_sample_r,
                   std::uint32_t center_sample_g,
                   std::uint32_t center_sample_b,
                   std::uint32_t corner_sample_r,
                   std::uint32_t corner_sample_g,
                   std::uint32_t corner_sample_b)
{
    if (!WritePpm(ppm_path, pixels, kWidth, kHeight))
    {
        return false;
    }

    std::ofstream output(json_path, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "{\n";
    output << "  \"task\": \"P4.6.11\",\n";
    output << "  \"scene\": \"bridge draw path triangle\",\n";
    output << "  \"presented_frames\": " << presented_frames << ",\n";
    output << "  \"center_sample\": [" << center_sample_r << ", " << center_sample_g << ", " << center_sample_b << "],\n";
    output << "  \"corner_sample\": [" << corner_sample_r << ", " << corner_sample_g << ", " << corner_sample_b << "],\n";
    output << "  \"artifacts\": [\"" << JsonEscape(ppm_path.string()) << "\", \"" << JsonEscape(json_path.string()) << "\"]\n";
    output << "}\n";
    return static_cast<bool>(output);
}

bool RunBridgeTriangle(double auto_close_seconds,
                       const std::filesystem::path& export_ppm_path,
                       const std::filesystem::path& export_json_path)
{
    @autoreleasepool
    {
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        metal_device* device = nullptr;
        if (metal_create_device(&device) != METAL_RESULT_OK || device == nullptr)
        {
            std::cerr << "无法创建 metal_device。\n";
            return false;
        }

        metal_queue* queue = nullptr;
        if (metal_create_queue(device, &queue) != METAL_RESULT_OK || queue == nullptr)
        {
            std::cerr << "无法创建 metal_queue。\n";
            metal_release(device);
            return false;
        }

        metal_shader_compiler* compiler = nullptr;
        if (metal_acquire_shader_compiler(device, &compiler) != METAL_RESULT_OK || compiler == nullptr)
        {
            std::cerr << "无法获取 shader compiler。\n";
            metal_release(queue);
            metal_release(device);
            return false;
        }

        ShaderBlob vs = CompileStage(compiler, kVertexSource, "vertex", "vs_main", "sm_6_0", "vertex");
        ShaderBlob fs = CompileStage(compiler, kFragmentSource, "fragment", "fs_main", "ps_6_0", "fragment");
        if (vs.data == nullptr || fs.data == nullptr)
        {
            metal_release(compiler);
            metal_release(queue);
            metal_release(device);
            return false;
        }

        metal_render_pipeline_descriptor pipeline_desc{};
        pipeline_desc.abi_version = METAL_BRIDGE_ABI_VERSION;
        pipeline_desc.vertex_metallib_data = vs.data;
        pipeline_desc.vertex_metallib_size = vs.size;
        pipeline_desc.fragment_metallib_data = fs.data;
        pipeline_desc.fragment_metallib_size = fs.size;
        pipeline_desc.vertex_function = "vs_main";
        pipeline_desc.fragment_function = "fs_main";
        pipeline_desc.color_attachment_format = METAL_PIXEL_FORMAT_BGRA8_UNORM;
        pipeline_desc.depth_stencil_format = METAL_PIXEL_FORMAT_INVALID;
        pipeline_desc.vertex_attribute_count = 0;
        pipeline_desc.vertex_buffer_layout_count = 0;
        pipeline_desc.blend_attachments = nullptr;
        pipeline_desc.blend_attachment_count = 0;
        pipeline_desc.reserved = 0;

        metal_render_pipeline* pipeline = nullptr;
        if (metal_create_render_pipeline(device, &pipeline_desc, &pipeline) != METAL_RESULT_OK || pipeline == nullptr)
        {
            std::cerr << "无法创建 render pipeline。\n";
            metal_release(compiler);
            metal_release(queue);
            metal_release(device);
            return false;
        }

        const NSRect frame = NSMakeRect(0.0, 0.0, static_cast<CGFloat>(kWidth), static_cast<CGFloat>(kHeight));
        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                  NSWindowStyleMaskClosable |
                                                                  NSWindowStyleMaskMiniaturizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        [window setTitle:@"P4.6.11 Bridge Triangle"];
        [window center];

        NSView* content_view = [[NSView alloc] initWithFrame:frame];
        [content_view setWantsLayer:YES];
        CAMetalLayer* layer = [CAMetalLayer layer];
        if (layer == nil)
        {
            std::cerr << "无法创建 CAMetalLayer。\n";
            metal_release(pipeline);
            metal_release(compiler);
            metal_release(queue);
            metal_release(device);
            return false;
        }

        layer.framebufferOnly = NO;
        layer.drawableSize = CGSizeMake(kWidth, kHeight);
        layer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        [content_view setLayer:layer];
        [window setContentView:content_view];
        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        metal_presenter* presenter = nullptr;
        if (metal_create_presenter(device, (__bridge void*)layer, &presenter) != METAL_RESULT_OK || presenter == nullptr)
        {
            std::cerr << "无法创建 presenter。\n";
            metal_release(pipeline);
            metal_release(compiler);
            metal_release(queue);
            metal_release(device);
            return false;
        }

        if (metal_presenter_resize(presenter, kWidth, kHeight) != METAL_RESULT_OK)
        {
            std::cerr << "无法调整 presenter 大小。\n";
            metal_release(presenter);
            metal_release(pipeline);
            metal_release(compiler);
            metal_release(queue);
            metal_release(device);
            return false;
        }

        metal_texture* render_target = nullptr;
        if (metal_create_texture(device,
                                 METAL_PIXEL_FORMAT_BGRA8_UNORM,
                                 kWidth,
                                 kHeight,
                                 1,
                                 1,
                                 1,
                                 METAL_TEXTURE_TYPE_2D,
                                 METAL_TEXTURE_USAGE_RENDER_TARGET | METAL_TEXTURE_USAGE_SHADER_READ,
                                 METAL_STORAGE_MODE_SHARED,
                                 &render_target) != METAL_RESULT_OK || render_target == nullptr)
        {
            std::cerr << "无法创建渲染目标纹理。\n";
            metal_release(presenter);
            metal_release(pipeline);
            metal_release(compiler);
            metal_release(queue);
            metal_release(device);
            return false;
        }

        metal_buffer* frame_buffer = nullptr;
        if (metal_create_buffer(device, sizeof(FrameData), METAL_STORAGE_MODE_SHARED, &frame_buffer) != METAL_RESULT_OK || frame_buffer == nullptr)
        {
            std::cerr << "无法创建帧数据缓冲区。\n";
            metal_release(render_target);
            metal_release(presenter);
            metal_release(pipeline);
            metal_release(compiler);
            metal_release(queue);
            metal_release(device);
            return false;
        }

        metal_buffer* readback_buffer = nullptr;
        if (metal_create_buffer(device,
                                static_cast<std::uint64_t>(kWidth) * kHeight * sizeof(Pixel),
                                METAL_STORAGE_MODE_SHARED,
                                &readback_buffer) != METAL_RESULT_OK || readback_buffer == nullptr)
        {
            std::cerr << "无法创建回读缓冲区。\n";
            metal_release(frame_buffer);
            metal_release(render_target);
            metal_release(presenter);
            metal_release(pipeline);
            metal_release(compiler);
            metal_release(queue);
            metal_release(device);
            return false;
        }

        const auto start_time = std::chrono::steady_clock::now();
        std::uint32_t presented_frames = 0;
        bool exported_frame = false;

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

                void* frame_ptr = nullptr;
                if (metal_map_buffer(frame_buffer, &frame_ptr) != METAL_RESULT_OK || frame_ptr == nullptr)
                {
                    std::cerr << "无法映射帧数据缓冲区。\n";
                    break;
                }

                FrameData* frame_data = static_cast<FrameData*>(frame_ptr);
                frame_data->phase = static_cast<float>(presented_frames) * 0.16f;
                frame_data->pad[0] = frame_data->pad[1] = frame_data->pad[2] = 0.0f;
                metal_unmap_buffer(frame_buffer);

                metal_command_buffer* command_buffer = nullptr;
                if (metal_begin_command_buffer(queue, &command_buffer) != METAL_RESULT_OK || command_buffer == nullptr)
                {
                    std::cerr << "无法创建 command buffer。\n";
                    break;
                }

                metal_color_attachment_descriptor color_attachment{};
                color_attachment.texture = render_target;
                color_attachment.level = 0;
                color_attachment.slice = 0;
                color_attachment.load_action = METAL_LOAD_ACTION_CLEAR;
                color_attachment.store_action = METAL_STORE_ACTION_STORE;
                color_attachment.clear_color = {0.04, 0.06, 0.10, 1.0};

                metal_render_encoder* encoder = nullptr;
                if (metal_begin_render_encoding_with_targets(
                        command_buffer,
                        pipeline,
                        &color_attachment,
                        1,
                        nullptr,
                        &encoder) != METAL_RESULT_OK || encoder == nullptr)
                {
                    std::cerr << "无法创建 render encoder。\n";
                    metal_release(command_buffer);
                    break;
                }

                if (metal_render_encoder_set_vertex_buffer(encoder, 0, frame_buffer, 0) != METAL_RESULT_OK)
                {
                    std::cerr << "无法绑定帧数据缓冲区。\n";
                    metal_release(encoder);
                    metal_release(command_buffer);
                    break;
                }

                if (metal_render_encoder_draw_primitives(
                        encoder,
                        METAL_PRIMITIVE_TYPE_TRIANGLE,
                        0,
                        3,
                        1,
                        0) != METAL_RESULT_OK)
                {
                    std::cerr << "无法提交 draw call。\n";
                    metal_release(encoder);
                    metal_release(command_buffer);
                    break;
                }

                metal_end_render_encoding(encoder);
                metal_release(encoder);

                if (metal_commit_command_buffer(command_buffer) != METAL_RESULT_OK)
                {
                    std::cerr << "无法提交 command buffer。\n";
                    metal_release(command_buffer);
                    break;
                }

                if (metal_wait_command_buffer(command_buffer) != METAL_RESULT_OK)
                {
                    std::cerr << "等待 command buffer 失败。\n";
                    metal_release(command_buffer);
                    break;
                }

                if (metal_texture_readback(
                        render_target,
                        readback_buffer,
                        0,
                        0,
                        0,
                        kWidth * sizeof(Pixel)) != METAL_RESULT_OK)
                {
                    std::cerr << "回读渲染目标失败。\n";
                    metal_release(command_buffer);
                    break;
                }

                if (metal_presenter_present_texture(presenter, render_target) != METAL_RESULT_OK)
                {
                    std::cerr << "窗口呈现失败: " << metal_get_last_error_message() << "\n";
                    metal_release(command_buffer);
                    break;
                }

                if (!exported_frame && !export_ppm_path.empty())
                {
                    std::vector<Pixel> pixels = ReadbackToPixels(readback_buffer);
                    if (pixels.empty())
                    {
                        std::cerr << "无法读取回读像素。\n";
                        metal_release(command_buffer);
                        break;
                    }

                    const Pixel corner = pixels.front();
                    const Pixel center = pixels[(kHeight / 2) * kWidth + (kWidth / 2)];
                    std::filesystem::create_directories(export_ppm_path.parent_path());
                    if (!WriteEvidence(export_ppm_path,
                                       export_json_path,
                                       pixels,
                                       presented_frames + 1,
                                       center.r,
                                       center.g,
                                       center.b,
                                       corner.r,
                                       corner.g,
                                       corner.b))
                    {
                        std::cerr << "无法写出证据文件。\n";
                        metal_release(command_buffer);
                        break;
                    }

                    std::cout << "已导出真实 draw path 首帧: " << export_ppm_path << "\n";
                    exported_frame = true;
                }

                metal_release(command_buffer);
                ++presented_frames;
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

        std::cout << "P4.6.11 真实 draw path smoke 完成: presents=" << presented_frames << "\n";

        metal_release(readback_buffer);
        metal_release(frame_buffer);
        metal_release(render_target);
        metal_release(presenter);
        metal_release(pipeline);
        metal_release(compiler);
        metal_release(queue);
        metal_release(device);
        return presented_frames > 0;
    }
}
} // namespace

int main(int argc, char** argv)
{
    double auto_close_seconds = 2.0;
    std::filesystem::path export_ppm_path;
    std::filesystem::path export_json_path;

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
        if (argument == "--export-json" && index + 1 < argc)
        {
            export_json_path = argv[++index];
            continue;
        }
    }

    if (export_json_path.empty() && !export_ppm_path.empty())
    {
        export_json_path = export_ppm_path;
        export_json_path.replace_extension(".json");
    }

    if (!RunBridgeTriangle(auto_close_seconds, export_ppm_path, export_json_path))
    {
        return 1;
    }

    return 0;
}
