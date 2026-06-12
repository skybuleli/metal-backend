// D8 Complex Showcase — 离屏渲染（输出 PPM 文件）
// 着色器路径：计算着色器 Path A (Slang→DXIL→MSC→metallib)，渲染着色器 inline MSL
// 场景：7 个 PBR 球体 (不同粗糙度/金属度) + 地面 + 程序化天空盒 + 粒子漩涡
// PRIVATE_IMPLEMENTATION 宏在 metal_cpp_impl.cpp 中定义

#include "d8_core.h"

using namespace d8;

// ═══════════════════════════════════════════════════════════════════════
// 主入口（离屏渲染 → PPM + 性能 JSON）
// ═══════════════════════════════════════════════════════════════════════
int main()
{
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    // ── 初始化渲染上下文 ──
    RenderContext ctx = {};
    if (!SetupRenderContext(ctx))
    {
        pool->drain();
        return 1;
    }

    // 离屏输出纹理（共享存储，用于读回像素）
    MTL::Texture* compositeTexture = MakeColorTexture(ctx.device,
        MTL::PixelFormatBGRA8Unorm, kWidth, kHeight,
        MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead, MTL::StorageModeShared);

    constexpr int kItemCount = static_cast<int>(std::size(kSceneItems));

    // ── 渲染循环 ──
    const auto begin = std::chrono::steady_clock::now();
    std::uint32_t minInstanceCount = kParticleCount;
    std::uint32_t maxInstanceCount = 0;
    std::uint32_t finalInstanceCount = 0;

    std::cout << "D8 粒子数: " << kParticleCount << "\n";
    std::cout << "场景物品: " << kItemCount << " 个 PBR 球体 + 地面 + 天空盒\n";
    std::cout << "渲染通道: Shadow → Scene(PBR+Skybox) → Bloom(3pass) → Composite → Particles → HUD\n";
    std::cout << "采样帧数: " << kSampleFrames << "\n";

    for (std::uint32_t frameIndex = 0; frameIndex < kSampleFrames; ++frameIndex)
    {
        const float time = static_cast<float>(frameIndex) * kDeltaTime;
        RenderFrame(ctx, compositeTexture, time, frameIndex);

        // 统计粒子间接绘制参数
        const DrawArgs* currentArgs = static_cast<const DrawArgs*>(ctx.indirectArgsBuffer->contents());
        minInstanceCount = std::min(minInstanceCount, currentArgs->instanceCount);
        maxInstanceCount = std::max(maxInstanceCount, currentArgs->instanceCount);
        finalInstanceCount = currentArgs->instanceCount;
    }

    // ── 性能统计 ──
    const auto end = std::chrono::steady_clock::now();
    const double sampleSeconds = std::chrono::duration_cast<std::chrono::duration<double>>(end - begin).count();
    const double avgFrameMs = sampleSeconds * 1000.0 / static_cast<double>(kSampleFrames);
    const double avgFps = static_cast<double>(kSampleFrames) / sampleSeconds;

    // ── 读回最终帧缓冲 ──
    std::vector<Pixel> pixels(kWidth * kHeight);
    compositeTexture->getBytes(
        pixels.data(),
        static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
        MTL::Region::Make2D(0, 0, kWidth, kHeight),
        0);

    // ── 写出 PPM ──
    std::filesystem::create_directories("out");
    if (!WritePpm(std::filesystem::path("out") / "complex_showcase.ppm", pixels))
    {
        std::cerr << "无法写出 D8 渲染结果。\n";
        compositeTexture->release();
        CleanupRenderContext(ctx);
        pool->drain();
        return 1;
    }

    // ── 写出性能 JSON ──
    const std::string deviceName = ctx.device->name()->utf8String();
    const PerfStats stats = {
        .avgFps = avgFps,
        .avgFrameMs = avgFrameMs,
        .sampleSeconds = sampleSeconds,
        .minInstanceCount = minInstanceCount,
        .maxInstanceCount = maxInstanceCount,
        .finalInstanceCount = finalInstanceCount,
    };
    if (!WritePerfJson(std::filesystem::path("out") / "complex_showcase_perf.json", deviceName, stats))
    {
        std::cerr << "无法写出 D8 性能记录。\n";
        compositeTexture->release();
        CleanupRenderContext(ctx);
        pool->drain();
        return 1;
    }

    std::cout << "\nD8 Complex Showcase 完成: out/complex_showcase.ppm\n";
    std::cout << std::fixed << std::setprecision(2)
              << "平均 FPS: " << avgFps
              << ", 平均帧时间: " << avgFrameMs << " ms"
              << ", 粒子 instanceCount 范围: [" << minInstanceCount << ", " << maxInstanceCount << "]\n";
    std::cout << "渲染通道: Shadow → Scene(PBR+Skybox) → Bloom(3pass) → Composite → Particles → HUD\n";
    std::cout << "PBR 模型: Cook-Torrance GGX, " << kItemCount << " 个球体 "
              << "(Gold/Chrome/Copper/Plastic/Ceramic/Blue Metal/Green Metal)\n";
    std::cout << "摄像机: 自由轨道 (自动旋转 + 高度振荡)\n";

    // ── 资源释放 ──
    compositeTexture->release();
    CleanupRenderContext(ctx);
    pool->drain();
    return 0;
}
