#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define IR_RUNTIME_METALCPP
#define IR_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>
#include <metal_irconverter_runtime/metal_irconverter_runtime.h>

#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

namespace
{
constexpr std::uint32_t kWidth = 960;
constexpr std::uint32_t kHeight = 720;
constexpr std::uint32_t kParticleCount = 4096;
constexpr std::uint32_t kThreadgroupSize = 64;
constexpr std::uint32_t kSampleFrames = 180;
constexpr float kDeltaTime = 1.0f / 60.0f;
constexpr const char* kComputeMetallibPath = "build/d7_particle_update.metallib";

struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

struct alignas(16) Particle
{
    float position[2];
    float velocity[2];
    float color[4];
    float size;
    float age;
    float lifetime;
    float phase;
};

struct alignas(16) DrawArgs
{
    std::uint32_t vertexCount;
    std::uint32_t instanceCount;
    std::uint32_t vertexStart;
    std::uint32_t baseInstance;
};

struct alignas(16) SimParams
{
    std::uint32_t particleCount;
    float deltaTime;
    float time;
    float aspect;
    std::uint32_t frameIndex;
    float speedScale;
    float drag;
    float verticalLimit;
};

struct alignas(16) RenderParams
{
    float aspect;
    float time;
    float intensity;
    float pad;
};

struct PerfStats
{
    double avgFps;
    double avgFrameMs;
    double sampleSeconds;
    std::uint32_t minInstanceCount;
    std::uint32_t maxInstanceCount;
    std::uint32_t finalInstanceCount;
};

static_assert(sizeof(Particle) == 48, "Particle 大小必须为 48 字节");
static_assert(sizeof(DrawArgs) == 16, "DrawArgs 大小必须为 16 字节");
static_assert(sizeof(SimParams) == 32, "SimParams 大小必须为 32 字节");
static_assert(sizeof(RenderParams) == 16, "RenderParams 大小必须为 16 字节");

const char* kRenderShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct Particle
{
    float2 position;
    float2 velocity;
    float4 color;
    float size;
    float age;
    float lifetime;
    float phase;
};

struct RenderParams
{
    float aspect;
    float time;
    float intensity;
    float pad;
};

struct VertexOut
{
    float4 position [[position]];
    float4 color;
    float2 localUv;
};

vertex VertexOut particleVertex(uint vertexId [[vertex_id]],
                                uint instanceId [[instance_id]],
                                const device Particle* particles [[buffer(0)]],
                                constant RenderParams& params [[buffer(1)]])
{
    float2 quad[6] = {
        float2(-1.0, -1.0),
        float2( 1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0, -1.0),
        float2( 1.0,  1.0),
        float2(-1.0,  1.0),
    };

    Particle particle = particles[instanceId];
    float life = clamp(1.0 - particle.age / max(particle.lifetime, 0.001), 0.0, 1.0);
    float pulse = 0.65 + 0.35 * sin(params.time * 3.1 + particle.phase * 2.0);
    float size = particle.size * mix(0.75, 1.35, life) * pulse * params.intensity;

    VertexOut out;
    out.localUv = quad[vertexId];
    float2 offset = quad[vertexId] * float2(size / params.aspect, size);
    out.position = float4(particle.position + offset, 0.0, 1.0);
    out.color = float4(particle.color.rgb * (0.30 + 0.70 * life) * (0.65 + 0.35 * pulse), life);
    return out;
}

fragment float4 particleFragment(VertexOut in [[stage_in]])
{
    float radius = length(in.localUv);
    float glow = smoothstep(1.0, 0.0, radius);
    float core = smoothstep(0.40, 0.0, radius);
    float alpha = glow * in.color.a;
    float3 color = in.color.rgb * (0.35 + core * 1.25 + glow * 0.40);
    return float4(color * alpha, alpha);
}
)";

std::string ErrorToString(NS::Error* error)
{
    if (error == nullptr)
    {
        return "未知错误";
    }

    NS::String* description = error->localizedDescription();
    return description != nullptr ? description->utf8String() : "未知错误";
}

bool WritePpm(const std::filesystem::path& outputPath, const std::vector<Pixel>& pixels)
{
    std::ofstream output(outputPath, std::ios::binary);
    if (!output)
    {
        return false;
    }

    output << "P6\n" << kWidth << " " << kHeight << "\n255\n";
    for (std::uint32_t y = 0; y < kHeight; ++y)
    {
        const std::uint32_t srcY = kHeight - 1 - y;
        for (std::uint32_t x = 0; x < kWidth; ++x)
        {
            const Pixel& pixel = pixels[srcY * kWidth + x];
            output.put(static_cast<char>(pixel.r));
            output.put(static_cast<char>(pixel.g));
            output.put(static_cast<char>(pixel.b));
        }
    }

    return true;
}

MTL::Library* LoadMetallib(MTL::Device* device, const char* path)
{
    NS::Error* error = nullptr;
    NS::String* libraryPath = NS::String::string(path, NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(libraryPath, &error);
    if (library == nullptr)
    {
        std::cerr << "无法加载 metallib " << path << ": " << ErrorToString(error) << "\n";
    }
    return library;
}

MTL::Buffer* MakeSharedBuffer(MTL::Device* device, const void* bytes, std::size_t size)
{
    return device->newBuffer(bytes, static_cast<NS::UInteger>(size), MTL::ResourceStorageModeShared);
}

MTL::Texture* MakeColorTexture(MTL::Device* device)
{
    MTL::TextureDescriptor* descriptor =
        MTL::TextureDescriptor::texture2DDescriptor(MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, false);
    descriptor->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    descriptor->setStorageMode(MTL::StorageModeShared);
    return device->newTexture(descriptor);
}

IRBufferView MakeBufferView(MTL::Buffer* buffer)
{
    return {
        .buffer = buffer,
        .bufferOffset = 0,
        .bufferSize = static_cast<std::uint64_t>(buffer->length()),
        .textureBufferView = nullptr,
        .textureViewOffsetInElements = 0,
        .typedBuffer = false,
    };
}

std::vector<Particle> BuildInitialParticles()
{
    std::vector<Particle> particles(kParticleCount);
    std::mt19937 rng(20260612u);
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    const float aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight);
    for (std::uint32_t index = 0; index < kParticleCount; ++index)
    {
        const float t = static_cast<float>(index) / static_cast<float>(kParticleCount);
        const float angle = t * 6.2831853f * 12.0f + unit(rng) * 0.8f;
        const float radius = 0.18f + 0.95f * std::sqrt(unit(rng));
        const float speed = 0.11f + unit(rng) * 0.13f;

        Particle particle = {};
        particle.position[0] = std::cos(angle) * radius * aspect;
        particle.position[1] = std::sin(angle) * radius;
        particle.velocity[0] = -std::sin(angle) * speed;
        particle.velocity[1] = std::cos(angle) * speed;
        particle.color[0] = 0.25f + 0.75f * unit(rng);
        particle.color[1] = 0.35f + 0.60f * unit(rng);
        particle.color[2] = 0.55f + 0.45f * unit(rng);
        particle.color[3] = 1.0f;
        particle.size = 0.010f + unit(rng) * 0.020f;
        particle.age = unit(rng) * 2.0f;
        particle.lifetime = 1.8f + unit(rng) * 2.8f;
        particle.phase = angle;
        particles[index] = particle;
    }

    return particles;
}

bool WritePerfJson(const std::filesystem::path& outputPath,
                   const std::string& deviceName,
                   const PerfStats& stats)
{
    std::ofstream output(outputPath);
    if (!output)
    {
        return false;
    }

    output << std::fixed << std::setprecision(4);
    output << "{\n";
    output << "  \"task\": \"P2.9\",\n";
    output << "  \"demo\": \"D7\",\n";
    output << "  \"device\": \"" << deviceName << "\",\n";
    output << "  \"particle_count\": " << kParticleCount << ",\n";
    output << "  \"sample_frames\": " << kSampleFrames << ",\n";
    output << "  \"sample_seconds\": " << stats.sampleSeconds << ",\n";
    output << "  \"avg_fps\": " << stats.avgFps << ",\n";
    output << "  \"avg_frame_ms\": " << stats.avgFrameMs << ",\n";
    output << "  \"cpu_submits_per_frame\": 2,\n";
    output << "  \"indirect_draws_per_frame\": 1,\n";
    output << "  \"min_instance_count\": " << stats.minInstanceCount << ",\n";
    output << "  \"max_instance_count\": " << stats.maxInstanceCount << ",\n";
    output << "  \"final_instance_count\": " << stats.finalInstanceCount << "\n";
    output << "}\n";
    return true;
}

MTL::ComputePipelineState* BuildComputePipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* function = library->newFunction(MTLSTR("main"));
    if (function == nullptr)
    {
        std::cerr << "无法从 compute metallib 中取出入口函数 main。\n";
        return nullptr;
    }

    MTL::ComputePipelineState* pipeline = device->newComputePipelineState(function, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 ComputePipelineState: " << ErrorToString(error) << "\n";
    }
    return pipeline;
}

MTL::RenderPipelineState* BuildRenderPipeline(MTL::Device* device, MTL::Library* library)
{
    NS::Error* error = nullptr;
    MTL::Function* vertexFunction = library->newFunction(MTLSTR("particleVertex"));
    MTL::Function* fragmentFunction = library->newFunction(MTLSTR("particleFragment"));
    if (vertexFunction == nullptr || fragmentFunction == nullptr)
    {
        std::cerr << "无法从内嵌 MSL 中取出粒子渲染入口。\n";
        return nullptr;
    }

    MTL::RenderPipelineDescriptor* descriptor = MTL::RenderPipelineDescriptor::alloc()->init();
    descriptor->setVertexFunction(vertexFunction);
    descriptor->setFragmentFunction(fragmentFunction);
    descriptor->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    descriptor->colorAttachments()->object(0)->setBlendingEnabled(true);
    descriptor->colorAttachments()->object(0)->setRgbBlendOperation(MTL::BlendOperationAdd);
    descriptor->colorAttachments()->object(0)->setAlphaBlendOperation(MTL::BlendOperationAdd);
    descriptor->colorAttachments()->object(0)->setSourceRGBBlendFactor(MTL::BlendFactorOne);
    descriptor->colorAttachments()->object(0)->setDestinationRGBBlendFactor(MTL::BlendFactorOne);
    descriptor->colorAttachments()->object(0)->setSourceAlphaBlendFactor(MTL::BlendFactorOne);
    descriptor->colorAttachments()->object(0)->setDestinationAlphaBlendFactor(MTL::BlendFactorOneMinusSourceAlpha);

    MTL::RenderPipelineState* pipeline = device->newRenderPipelineState(descriptor, &error);
    if (pipeline == nullptr)
    {
        std::cerr << "无法创建 RenderPipelineState: " << ErrorToString(error) << "\n";
    }
    return pipeline;
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

    const std::string deviceName = device->name()->utf8String();
    std::cout << "设备: " << deviceName << "\n";

    MTL::CommandQueue* commandQueue = device->newCommandQueue();
    if (commandQueue == nullptr)
    {
        std::cerr << "无法创建 MTLCommandQueue。\n";
        pool->drain();
        return 1;
    }

    MTL::Library* computeLibrary = LoadMetallib(device, kComputeMetallibPath);
    if (computeLibrary == nullptr)
    {
        pool->drain();
        return 1;
    }
    std::cout << "Path A compute metallib 加载通过\n";

    NS::Error* error = nullptr;
    NS::String* renderSource = NS::String::string(kRenderShaderSource, NS::UTF8StringEncoding);
    MTL::Library* renderLibrary = device->newLibrary(renderSource, nullptr, &error);
    if (renderLibrary == nullptr)
    {
        std::cerr << "粒子渲染 MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }

    MTL::ComputePipelineState* computePipeline = BuildComputePipeline(device, computeLibrary);
    MTL::RenderPipelineState* renderPipeline = BuildRenderPipeline(device, renderLibrary);
    if (computePipeline == nullptr || renderPipeline == nullptr)
    {
        pool->drain();
        return 1;
    }

    std::vector<Particle> initialParticles = BuildInitialParticles();
    DrawArgs initialArgs = {6u, 0u, 0u, 0u};
    SimParams simParams = {
        .particleCount = kParticleCount,
        .deltaTime = kDeltaTime,
        .time = 0.0f,
        .aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight),
        .frameIndex = 0,
        .speedScale = 1.0f,
        .drag = 0.92f,
        .verticalLimit = 1.18f,
    };
    RenderParams renderParams = {
        .aspect = static_cast<float>(kWidth) / static_cast<float>(kHeight),
        .time = 0.0f,
        .intensity = 1.0f,
        .pad = 0.0f,
    };

    MTL::Buffer* particleBuffer = MakeSharedBuffer(device, initialParticles.data(), initialParticles.size() * sizeof(Particle));
    MTL::Buffer* indirectArgsBuffer = MakeSharedBuffer(device, &initialArgs, sizeof(initialArgs));
    MTL::Buffer* simParamsBuffer = MakeSharedBuffer(device, &simParams, sizeof(simParams));
    MTL::Buffer* renderParamsBuffer = MakeSharedBuffer(device, &renderParams, sizeof(renderParams));
    if (particleBuffer == nullptr || indirectArgsBuffer == nullptr || simParamsBuffer == nullptr || renderParamsBuffer == nullptr)
    {
        std::cerr << "无法创建 D7 所需缓冲区。\n";
        pool->drain();
        return 1;
    }

    MTL::Texture* colorTexture = MakeColorTexture(device);
    if (colorTexture == nullptr)
    {
        std::cerr << "无法创建 D7 离屏颜色纹理。\n";
        pool->drain();
        return 1;
    }

    const IRBufferView particleBufferView = MakeBufferView(particleBuffer);
    const IRBufferView indirectArgsBufferView = MakeBufferView(indirectArgsBuffer);
    const IRBufferView simParamsBufferView = MakeBufferView(simParamsBuffer);
    IRDescriptorTableEntry computeEntries[3] = {};
    IRDescriptorTableSetBufferView(&computeEntries[0], &particleBufferView);
    IRDescriptorTableSetBufferView(&computeEntries[1], &indirectArgsBufferView);
    IRDescriptorTableSetBufferView(&computeEntries[2], &simParamsBufferView);
    MTL::Buffer* computeArgumentBuffer = MakeSharedBuffer(device, computeEntries, sizeof(computeEntries));
    if (computeArgumentBuffer == nullptr)
    {
        std::cerr << "无法创建 D7 compute 参数缓冲。\n";
        pool->drain();
        return 1;
    }

    const auto begin = std::chrono::steady_clock::now();
    std::uint32_t minInstanceCount = kParticleCount;
    std::uint32_t maxInstanceCount = 0;
    std::uint32_t finalInstanceCount = 0;

    std::cout << "D7 粒子数: " << kParticleCount << "\n";
    std::cout << "CPU 每帧提交: compute dispatch 1 次 + indirect draw 1 次\n";

    for (std::uint32_t frameIndex = 0; frameIndex < kSampleFrames; ++frameIndex)
    {
        simParams.time = static_cast<float>(frameIndex) * kDeltaTime;
        simParams.frameIndex = frameIndex;
        std::memcpy(simParamsBuffer->contents(), &simParams, sizeof(simParams));

        renderParams.time = simParams.time;
        renderParams.intensity = 1.0f + 0.08f * std::sin(simParams.time * 1.7f);
        std::memcpy(renderParamsBuffer->contents(), &renderParams, sizeof(renderParams));

        MTL::CommandBuffer* commandBuffer = commandQueue->commandBuffer();
        if (commandBuffer == nullptr)
        {
            std::cerr << "无法创建 MTLCommandBuffer。\n";
            pool->drain();
            return 1;
        }

        MTL::ComputeCommandEncoder* computeEncoder = commandBuffer->computeCommandEncoder();
        computeEncoder->setComputePipelineState(computePipeline);
        computeEncoder->setBuffer(computeArgumentBuffer, 0, kIRArgumentBufferBindPoint);
        computeEncoder->dispatchThreadgroups(
            MTL::Size::Make((kParticleCount + kThreadgroupSize - 1) / kThreadgroupSize, 1, 1),
            MTL::Size::Make(kThreadgroupSize, 1, 1));
        computeEncoder->endEncoding();

        MTL::RenderPassDescriptor* passDescriptor = MTL::RenderPassDescriptor::alloc()->init();
        MTL::RenderPassColorAttachmentDescriptor* colorAttachment = passDescriptor->colorAttachments()->object(0);
        colorAttachment->setTexture(colorTexture);
        colorAttachment->setLoadAction(MTL::LoadActionClear);
        colorAttachment->setStoreAction(MTL::StoreActionStore);
        colorAttachment->setClearColor(MTL::ClearColor(0.02, 0.04, 0.07, 1.0));

        MTL::RenderCommandEncoder* renderEncoder = commandBuffer->renderCommandEncoder(passDescriptor);
        renderEncoder->setRenderPipelineState(renderPipeline);
        renderEncoder->setCullMode(MTL::CullModeNone);
        renderEncoder->setVertexBuffer(particleBuffer, 0, 0);
        renderEncoder->setVertexBuffer(renderParamsBuffer, 0, 1);
        renderEncoder->drawPrimitives(MTL::PrimitiveTypeTriangle, indirectArgsBuffer, 0);
        renderEncoder->endEncoding();

        commandBuffer->commit();
        commandBuffer->waitUntilCompleted();

        const DrawArgs* currentArgs = static_cast<const DrawArgs*>(indirectArgsBuffer->contents());
        minInstanceCount = std::min(minInstanceCount, currentArgs->instanceCount);
        maxInstanceCount = std::max(maxInstanceCount, currentArgs->instanceCount);
        finalInstanceCount = currentArgs->instanceCount;
    }

    const auto end = std::chrono::steady_clock::now();
    const double sampleSeconds =
        std::chrono::duration_cast<std::chrono::duration<double>>(end - begin).count();
    const double avgFrameMs = sampleSeconds * 1000.0 / static_cast<double>(kSampleFrames);
    const double avgFps = static_cast<double>(kSampleFrames) / sampleSeconds;

    std::vector<Pixel> pixels(kWidth * kHeight);
    colorTexture->getBytes(
        pixels.data(),
        static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
        MTL::Region::Make2D(0, 0, kWidth, kHeight),
        0);

    std::filesystem::create_directories("out");
    if (!WritePpm(std::filesystem::path("out") / "gpu_driven.ppm", pixels))
    {
        std::cerr << "无法写出 D7 离屏结果。\n";
        pool->drain();
        return 1;
    }

    const PerfStats stats = {
        .avgFps = avgFps,
        .avgFrameMs = avgFrameMs,
        .sampleSeconds = sampleSeconds,
        .minInstanceCount = minInstanceCount,
        .maxInstanceCount = maxInstanceCount,
        .finalInstanceCount = finalInstanceCount,
    };
    if (!WritePerfJson(std::filesystem::path("out") / "gpu_driven_perf.json", deviceName, stats))
    {
        std::cerr << "无法写出 D7 性能记录。\n";
        pool->drain();
        return 1;
    }

    std::cout << "D7 GPU-Driven 完成: out/gpu_driven.ppm\n";
    std::cout << std::fixed << std::setprecision(2)
              << "平均 FPS: " << avgFps
              << ", 平均帧时间: " << avgFrameMs << " ms"
              << ", indirect instanceCount 范围: [" << minInstanceCount << ", " << maxInstanceCount << "]\n";

    pool->drain();
    return 0;
}
