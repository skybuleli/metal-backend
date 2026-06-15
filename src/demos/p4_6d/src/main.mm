#import <AppKit/AppKit.h>
#import <QuartzCore/CAMetalLayer.h>

#include "metal_bridge.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace
{
constexpr std::uint32_t kWidth = 960;
constexpr std::uint32_t kHeight = 540;
constexpr std::uint32_t kBoardCols = 10;
constexpr std::uint32_t kBoardRows = 20;
constexpr std::uint32_t kBoardCell = 18;
constexpr std::uint32_t kBoardX = 48;
constexpr std::uint32_t kBoardY = 104;
constexpr std::uint32_t kCourtX = 504;
constexpr std::uint32_t kCourtY = 104;

struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

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

void BlendPixel(std::vector<Pixel>& pixels, std::uint32_t width, std::uint32_t x, std::uint32_t y, Pixel src)
{
    if (x >= width || y >= kHeight)
    {
        return;
    }

    Pixel& dst = pixels[y * width + x];
    const float alpha = static_cast<float>(src.a) / 255.0f;
    const float inv_alpha = 1.0f - alpha;
    dst.r = static_cast<std::uint8_t>(src.r * alpha + dst.r * inv_alpha);
    dst.g = static_cast<std::uint8_t>(src.g * alpha + dst.g * inv_alpha);
    dst.b = static_cast<std::uint8_t>(src.b * alpha + dst.b * inv_alpha);
    dst.a = 255;
}

void FillRect(std::vector<Pixel>& pixels,
              std::uint32_t width,
              std::uint32_t x,
              std::uint32_t y,
              std::uint32_t rect_width,
              std::uint32_t rect_height,
              Pixel color)
{
    const std::uint32_t max_x = std::min<std::uint32_t>(x + rect_width, width);
    const std::uint32_t max_y = std::min<std::uint32_t>(y + rect_height, kHeight);
    for (std::uint32_t py = y; py < max_y; ++py)
    {
        for (std::uint32_t px = x; px < max_x; ++px)
        {
            BlendPixel(pixels, width, px, py, color);
        }
    }
}

void DrawGrid(std::vector<Pixel>& pixels,
              std::uint32_t width,
              std::uint32_t x,
              std::uint32_t y,
              std::uint32_t cols,
              std::uint32_t rows,
              std::uint32_t cell,
              Pixel line,
              Pixel fill)
{
    FillRect(pixels, width, x, y, cols * cell, rows * cell, fill);
    for (std::uint32_t row = 0; row <= rows; ++row)
    {
        FillRect(pixels, width, x, y + row * cell, cols * cell, 1, line);
    }
    for (std::uint32_t col = 0; col <= cols; ++col)
    {
        FillRect(pixels, width, x + col * cell, y, 1, rows * cell, line);
    }
}

const std::array<const char*, 7>* PatternForChar(char ch)
{
    static constexpr std::array<const char*, 7> kA = {{"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}};
    static constexpr std::array<const char*, 7> kB = {{"####.", "#...#", "#...#", "####.", "#...#", "#...#", "####."}};
    static constexpr std::array<const char*, 7> kD = {{"####.", "#...#", "#...#", "#...#", "#...#", "#...#", "####."}};
    static constexpr std::array<const char*, 7> kE = {{"#####", "#....", "#....", "####.", "#....", "#....", "#####"}};
    static constexpr std::array<const char*, 7> kG = {{".###.", "#...#", "#....", "#.###", "#...#", "#...#", ".###."}};
    static constexpr std::array<const char*, 7> kH = {{"#...#", "#...#", "#...#", "#####", "#...#", "#...#", "#...#"}};
    static constexpr std::array<const char*, 7> kI = {{"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "#####"}};
    static constexpr std::array<const char*, 7> kK = {{"#...#", "#..#.", "#.#..", "##...", "#.#..", "#..#.", "#...#"}};
    static constexpr std::array<const char*, 7> kL = {{"#....", "#....", "#....", "#....", "#....", "#....", "#####"}};
    static constexpr std::array<const char*, 7> kM = {{"#...#", "##.##", "#.#.#", "#.#.#", "#...#", "#...#", "#...#"}};
    static constexpr std::array<const char*, 7> kN = {{"#...#", "##..#", "#.#.#", "#..##", "#...#", "#...#", "#...#"}};
    static constexpr std::array<const char*, 7> kO = {{".###.", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}};
    static constexpr std::array<const char*, 7> kP = {{"####.", "#...#", "#...#", "####.", "#....", "#....", "#...."}};
    static constexpr std::array<const char*, 7> kR = {{"####.", "#...#", "#...#", "####.", "#.#..", "#..#.", "#...#"}};
    static constexpr std::array<const char*, 7> kS = {{".###.", "#...#", "#....", ".###.", "....#", "#...#", ".###."}};
    static constexpr std::array<const char*, 7> kT = {{"#####", "..#..", "..#..", "..#..", "..#..", "..#..", "..#.."}};
    static constexpr std::array<const char*, 7> kU = {{"#...#", "#...#", "#...#", "#...#", "#...#", "#...#", ".###."}};
    static constexpr std::array<const char*, 7> kW = {{"#...#", "#...#", "#.#.#", "#.#.#", "##.##", "#...#", "#...#"}};

    switch (ch)
    {
        case 'A': return &kA;
        case 'B': return &kB;
        case 'D': return &kD;
        case 'E': return &kE;
        case 'G': return &kG;
        case 'H': return &kH;
        case 'I': return &kI;
        case 'K': return &kK;
        case 'L': return &kL;
        case 'M': return &kM;
        case 'N': return &kN;
        case 'O': return &kO;
        case 'P': return &kP;
        case 'R': return &kR;
        case 'S': return &kS;
        case 'T': return &kT;
        case 'U': return &kU;
        case 'W': return &kW;
        default: return nullptr;
    }
}

void DrawText(std::vector<Pixel>& pixels,
              std::uint32_t width,
              std::uint32_t x,
              std::uint32_t y,
              std::uint32_t scale,
              const std::string& text,
              Pixel color)
{
    std::uint32_t cursor = x;
    for (char ch : text)
    {
        if (ch == ' ')
        {
            cursor += 6 * scale;
            continue;
        }

        const std::array<const char*, 7>* glyph = PatternForChar(ch);
        if (glyph != nullptr)
        {
            for (std::uint32_t row = 0; row < 7; ++row)
            {
                for (std::uint32_t col = 0; col < 5; ++col)
                {
                    if ((*glyph)[row][col] == '#')
                    {
                        FillRect(pixels, width, cursor + col * scale, y + row * scale, scale, scale, color);
                    }
                }
            }
        }

        cursor += 6 * scale;
    }
}

bool BoardOccupied(std::uint32_t col, std::uint32_t row)
{
    if (row >= 18)
    {
        return true;
    }
    if (row == 17 && col >= 1 && col <= 8)
    {
        return true;
    }
    if (row == 16 && (col == 2 || col == 3 || col == 6 || col == 7))
    {
        return true;
    }
    if (row == 15 && (col == 3 || col == 4 || col == 5 || col == 6))
    {
        return true;
    }
    if (row == 13 && col >= 4 && col <= 6)
    {
        return true;
    }
    if (row == 11 && (col == 2 || col == 7))
    {
        return true;
    }
    if (row == 9 && col >= 4 && col <= 8)
    {
        return true;
    }
    if (row == 7 && (col == 0 || col == 9))
    {
        return true;
    }
    if (row == 5 && col >= 4 && col <= 5)
    {
        return true;
    }
    if (row == 10 && col == 5)
    {
        return true;
    }
    return false;
}

std::vector<Pixel> BuildFrame(std::uint32_t frame_index)
{
    std::vector<Pixel> pixels(kWidth * kHeight, Pixel{26, 20, 18, 255});

    for (std::uint32_t y = 0; y < kHeight; ++y)
    {
        const float t = static_cast<float>(y) / static_cast<float>(kHeight - 1);
        const std::uint8_t r = static_cast<std::uint8_t>(18 + t * 20.0f);
        const std::uint8_t g = static_cast<std::uint8_t>(24 + t * 32.0f);
        const std::uint8_t b = static_cast<std::uint8_t>(42 + t * 40.0f);
        for (std::uint32_t x = 0; x < kWidth; ++x)
        {
            pixels[y * kWidth + x] = Pixel{b, g, r, 255};
        }
    }

    FillRect(pixels, kWidth, 0, 0, kWidth, 88, Pixel{18, 18, 34, 230});
    FillRect(pixels, kWidth, 24, 18, 912, 58, Pixel{40, 54, 84, 210});
    FillRect(pixels, kWidth, 32, 96, 208, 384, Pixel{20, 24, 32, 255});
    FillRect(pixels, kWidth, 504, 96, 408, 384, Pixel{18, 30, 24, 255});

    const Pixel title = Pixel{242, 232, 210, 255};
    const Pixel subtitle = Pixel{200, 222, 255, 255};
    DrawText(pixels, kWidth, 44, 28, 2, "WINDOW PATH", title);
    DrawText(pixels, kWidth, 44, 52, 2, "SMOKE HOME BREW", subtitle);

    DrawGrid(pixels,
             kWidth,
             kBoardX,
             kBoardY,
             kBoardCols,
             kBoardRows,
             kBoardCell,
             Pixel{68, 78, 100, 255},
             Pixel{31, 37, 52, 255});

    for (std::uint32_t row = 0; row < kBoardRows; ++row)
    {
        for (std::uint32_t col = 0; col < kBoardCols; ++col)
        {
            if (!BoardOccupied(col, row))
            {
                continue;
            }

            const std::uint32_t x = kBoardX + col * kBoardCell + 2;
            const std::uint32_t y = kBoardY + row * kBoardCell + 2;
            const std::uint8_t hue = static_cast<std::uint8_t>((col * 23 + row * 11 + frame_index * 7) % 128);
            FillRect(pixels,
                     kWidth,
                     x,
                     y,
                     kBoardCell - 4,
                     kBoardCell - 4,
                     Pixel{
                         static_cast<std::uint8_t>(120 + hue),
                         static_cast<std::uint8_t>(95 + hue / 3),
                         static_cast<std::uint8_t>(70 + hue / 2),
                         255});
        }
    }

    for (std::uint32_t y = 0; y < 12; ++y)
    {
        FillRect(pixels, kWidth, kCourtX + 200, kCourtY + 12 + y * 28, 8, 18, Pixel{221, 233, 230, 220});
    }

    const std::uint32_t paddle_left_y = kCourtY + 140 + (frame_index % 20);
    const std::uint32_t paddle_right_y = kCourtY + 188 - (frame_index % 24);
    FillRect(pixels, kWidth, kCourtX + 20, paddle_left_y, 12, 72, Pixel{238, 228, 210, 255});
    FillRect(pixels, kWidth, kCourtX + 376, paddle_right_y, 12, 72, Pixel{238, 228, 210, 255});

    const std::uint32_t ball_x = kCourtX + 196 + (frame_index % 34);
    const std::uint32_t ball_y = kCourtY + 166 + ((frame_index * 3) % 24);
    FillRect(pixels, kWidth, ball_x, ball_y, 14, 14, Pixel{248, 214, 90, 255});

    FillRect(pixels, kWidth, 44, 478, 420, 26, Pixel{18, 26, 44, 190});
    DrawText(pixels, kWidth, 52, 484, 1, "WINDOW SMOKE", Pixel{241, 236, 220, 255});
    DrawText(pixels, kWidth, 512, 484, 1, "PRESENT PATH", Pixel{207, 229, 193, 255});

    return pixels;
}

struct AppContext
{
    metal_device* device = nullptr;
    metal_presenter* presenter = nullptr;
    metal_texture* texture = nullptr;
    metal_buffer* pixel_buffer = nullptr;
};

bool PresentFrame(AppContext& ctx, const std::vector<Pixel>& frame)
{
    if (ctx.texture == nullptr)
    {
        return false;
    }

    if (ctx.pixel_buffer != nullptr)
    {
        metal_release(ctx.pixel_buffer);
        ctx.pixel_buffer = nullptr;
    }

    if (metal_create_buffer_with_bytes(ctx.device,
                                       frame.data(),
                                       static_cast<std::uint64_t>(frame.size() * sizeof(Pixel)),
                                       METAL_STORAGE_MODE_SHARED,
                                       &ctx.pixel_buffer) != METAL_RESULT_OK)
    {
        std::cerr << "无法创建像素缓冲区。\n";
        return false;
    }

    if (metal_texture_upload(ctx.texture,
                             ctx.pixel_buffer,
                             0,
                             0,
                             0,
                             0,
                             0,
                             0,
                             kWidth,
                             kHeight,
                             kWidth * sizeof(Pixel)) != METAL_RESULT_OK)
    {
        std::cerr << "纹理上传失败: " << metal_get_last_error_message() << "\n";
        return false;
    }

    if (metal_presenter_present_texture(ctx.presenter, ctx.texture) != METAL_RESULT_OK)
    {
        std::cerr << "窗口呈现失败: " << metal_get_last_error_message() << "\n";
        return false;
    }

    return true;
}

bool RunWindowSmoke(double auto_close_seconds, const std::filesystem::path& export_ppm_path)
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

        const NSRect frame = NSMakeRect(0.0, 0.0, static_cast<CGFloat>(kWidth), static_cast<CGFloat>(kHeight));
        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                  NSWindowStyleMaskClosable |
                                                                  NSWindowStyleMaskMiniaturizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        [window setTitle:@"P4.6.10 Window Smoke"];
        [window center];

        NSView* content_view = [[NSView alloc] initWithFrame:frame];
        [content_view setWantsLayer:YES];

        CAMetalLayer* layer = [CAMetalLayer layer];
        if (layer == nullptr)
        {
            std::cerr << "无法创建 CAMetalLayer。\n";
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
            metal_release(device);
            return false;
        }

        if (metal_presenter_resize(presenter, kWidth, kHeight) != METAL_RESULT_OK)
        {
            std::cerr << "无法调整 presenter 尺寸。\n";
            metal_release(presenter);
            metal_release(device);
            return false;
        }

        metal_texture* texture = nullptr;
        if (metal_create_texture(device,
                                 METAL_PIXEL_FORMAT_BGRA8_UNORM,
                                 kWidth,
                                 kHeight,
                                 1,
                                 1,
                                 1,
                                 METAL_TEXTURE_TYPE_2D,
                                 METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
                                 METAL_STORAGE_MODE_SHARED,
                                 &texture) != METAL_RESULT_OK || texture == nullptr)
        {
            std::cerr << "无法创建呈现纹理。\n";
            metal_release(presenter);
            metal_release(device);
            return false;
        }

        AppContext context;
        context.device = device;
        context.presenter = presenter;
        context.texture = texture;
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

                const std::vector<Pixel> frame_pixels = BuildFrame(presented_frames);
                if (!exported_frame && !export_ppm_path.empty())
                {
                    std::filesystem::create_directories(export_ppm_path.parent_path());
                    if (!WritePpm(export_ppm_path, frame_pixels, kWidth, kHeight))
                    {
                        std::cerr << "无法导出窗口 smoke PPM: " << export_ppm_path << "\n";
                        break;
                    }
                    std::cout << "已导出窗口 smoke PPM: " << export_ppm_path << "\n";
                    exported_frame = true;
                }

                if (!PresentFrame(context, frame_pixels))
                {
                    break;
                }

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
        std::cout << "P4.6.10 窗口 smoke 完成: presents=" << presented_frames << "\n";
        metal_release(texture);
        metal_release(presenter);
        metal_release(device);
        return presented_frames > 0;
    }
}
} // namespace

int main(int argc, char** argv)
{
    double auto_close_seconds = 2.0;
    std::filesystem::path export_ppm_path;

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
    }

    if (!RunWindowSmoke(auto_close_seconds, export_ppm_path))
    {
        return 1;
    }

    return 0;
}
