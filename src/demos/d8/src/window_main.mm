// D8 Complex Showcase — 窗口实时显示
// 使用 NSWindow + CAMetalLayer 实时渲染 PBR 场景
// 不定义 PRIVATE_IMPLEMENTATION（由 metal_cpp_impl.o 提供 metal-cpp 符号）

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#include "d8_core.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

using namespace d8;

namespace
{

bool RunWindow(double autoCloseSeconds)
{
    @autoreleasepool
    {
        // ── 初始化渲染上下文（使用 metal-cpp 侧） ──
        RenderContext ctx = {};
        if (!SetupRenderContext(ctx))
        {
            return false;
        }

        std::cout << "D8 窗口模式: " << kWidth << "x" << kHeight << "\n";
        std::cout << "渲染通道: Shadow → Scene(PBR+Skybox) → Bloom(3pass) → Composite → Particles → HUD\n";

        // ── 创建 ObjC Metal 设备引用（复用 metal-cpp 已创建的 device） ──
        id<MTLDevice> objcDevice = (__bridge id<MTLDevice>)ctx.device;
        id<MTLCommandQueue> objcQueue = (__bridge id<MTLCommandQueue>)ctx.commandQueue;

        // ── NSApplication 初始化 ──
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];

        // ── 创建窗口 ──
        const NSRect frame = NSMakeRect(0.0, 0.0,
            static_cast<CGFloat>(kWidth), static_cast<CGFloat>(kHeight));
        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                      styleMask:(NSWindowStyleMaskTitled |
                                                                 NSWindowStyleMaskClosable |
                                                                 NSWindowStyleMaskMiniaturizable)
                                                        backing:NSBackingStoreBuffered
                                                          defer:NO];
        [window setTitle:@"D8 Complex Showcase — PBR + Shadow + Skybox + Bloom + Particles"];
        [window center];

        NSView* contentView = [[NSView alloc] initWithFrame:frame];
        [contentView setWantsLayer:YES];
        [window setContentView:contentView];

        // ── CAMetalLayer 配置 ──
        CAMetalLayer* metalLayer = [CAMetalLayer layer];
        metalLayer.device = objcDevice;
        metalLayer.pixelFormat = MTLPixelFormatBGRA8Unorm;
        metalLayer.framebufferOnly = NO;
        metalLayer.drawableSize = CGSizeMake(kWidth, kHeight);
        metalLayer.frame = contentView.bounds;
        metalLayer.contentsScale = window.backingScaleFactor;
        metalLayer.autoresizingMask = kCALayerWidthSizable | kCALayerHeightSizable;
        [contentView setLayer:metalLayer];

        [window makeKeyAndOrderFront:nil];
        [NSApp activateIgnoringOtherApps:YES];

        std::cout << "窗口已创建，实时渲染中... (关闭窗口或 Ctrl+C 退出)\n";
        if (autoCloseSeconds > 0.0)
        {
            std::cout << "自动关闭: " << autoCloseSeconds << " 秒后\n";
        }

        // ── 渲染循环 ──
        const auto startTime = std::chrono::steady_clock::now();
        std::uint32_t presentedFrames = 0;

        while (true)
        {
            @autoreleasepool
            {
                // 处理事件
                for (;;)
                {
                    NSEvent* event = [NSApp nextEventMatchingMask:NSEventMaskAny
                                                        untilDate:[NSDate distantPast]
                                                           inMode:NSDefaultRunLoopMode
                                                          dequeue:YES];
                    if (event == nil) break;
                    [NSApp sendEvent:event];
                    [NSApp updateWindows];
                }

                // 检查窗口是否可见
                if (![window isVisible]) break;

                // 获取 drawable
                id<CAMetalDrawable> drawable = [metalLayer nextDrawable];
                if (drawable != nil)
                {
                    // 计算实时时间
                    const auto now = std::chrono::steady_clock::now();
                    const float time = std::chrono::duration<float>(now - startTime).count();

                    // 使用 ObjC 侧 texture 包装为 metal-cpp MTL::Texture*
                    // CAMetalDrawable.texture 是 id<MTLTexture>，与 MTL::Texture* 二进制兼容
                    MTL::Texture* targetTexture = reinterpret_cast<MTL::Texture*>(
                        (__bridge void*)drawable.texture);

                    // 渲染一帧（所有 pass 最终输出到 targetTexture）
                    RenderFrame(ctx, targetTexture, time, presentedFrames);

                    // 提交并显示
                    id<MTLCommandBuffer> commandBuffer = [objcQueue commandBuffer];
                    [commandBuffer presentDrawable:drawable];
                    [commandBuffer commit];
                    [commandBuffer waitUntilCompleted];

                    ++presentedFrames;
                }
            }

            // 自动关闭检查
            if (autoCloseSeconds > 0.0)
            {
                const double elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime).count();
                if (elapsed >= autoCloseSeconds) break;
            }

            // ~60fps 限制
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        [window close];

        const auto endTime = std::chrono::steady_clock::now();
        const double totalSeconds = std::chrono::duration<double>(endTime - startTime).count();
        const double avgFps = (totalSeconds > 0.0) ? (presentedFrames / totalSeconds) : 0.0;

        std::cout << "已渲染: " << presentedFrames << " 帧"
                  << ", 平均 FPS: " << std::fixed << std::setprecision(1) << avgFps << "\n";
        std::cout << "窗口已关闭\n";

        // ── 资源释放 ──
        CleanupRenderContext(ctx);
        return presentedFrames > 0;
    }
}

} // namespace

int main(int argc, char** argv)
{
    double autoCloseSeconds = 0.0;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--auto-close-seconds" && i + 1 < argc)
        {
            autoCloseSeconds = std::stod(argv[++i]);
            continue;
        }
        if (arg == "--help" || arg == "-h")
        {
            std::cout << "用法: d8_window [--auto-close-seconds 秒]\n";
            std::cout << "  D8 Complex Showcase 窗口实时渲染\n";
            std::cout << "  --auto-close-seconds N  N 秒后自动关闭窗口\n";
            return 0;
        }
    }

    if (!RunWindow(autoCloseSeconds))
    {
        return 1;
    }

    return 0;
}
