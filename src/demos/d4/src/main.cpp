#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include <Foundation/Foundation.hpp>
#include <Metal/Metal.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace
{

// ============================================================================
// 渲染目标尺寸
// ============================================================================
constexpr std::uint32_t kWidth  = 512;
constexpr std::uint32_t kHeight = 512;

// ============================================================================
// 像素结构（BGRA 离屏读取）
// ============================================================================
struct Pixel
{
    std::uint8_t b;
    std::uint8_t g;
    std::uint8_t r;
    std::uint8_t a;
};

// ============================================================================
// 工具函数
// ============================================================================
std::string ErrorToString(NS::Error* error)
{
    if (error == nullptr) { return "未知错误"; }
    NS::String* desc = error->localizedDescription();
    return desc != nullptr ? desc->utf8String() : "未知错误";
}

bool WritePpm(const std::filesystem::path& output_path,
              const std::vector<Pixel>& pixels,
              std::uint32_t width, std::uint32_t height)
{
    std::ofstream output(output_path, std::ios::binary);
    if (!output) { return false; }
    output << "P6\n" << width << " " << height << "\n255\n";
    for (const Pixel& p : pixels)
    {
        output.put(static_cast<char>(p.r));
        output.put(static_cast<char>(p.g));
        output.put(static_cast<char>(p.b));
    }
    return true;
}

// ============================================================================
// 4×4 矩阵数学（列主序，与 Metal/MSL 对齐）
// ============================================================================
struct alignas(16) Mat4
{
    float m[16] = {}; // 列主序: m[col*4 + row]
};

// Mat4Identity — 不使用时注释掉以避免编译警告
// Mat4 Mat4Identity() { ... }

Mat4 Mat4Mul(const Mat4& a, const Mat4& b)
{
    Mat4 r;
    for (int col = 0; col < 4; ++col)
    {
        for (int row = 0; row < 4; ++row)
        {
            float sum = 0.0f;
            for (int k = 0; k < 4; ++k)
            {
                sum += a.m[k * 4 + row] * b.m[col * 4 + k];
            }
            r.m[col * 4 + row] = sum;
        }
    }
    return r;
}

Mat4 Mat4Perspective(float fovY, float aspect, float nearZ, float farZ)
{
    float tanHalf = std::tan(fovY * 0.5f);
    Mat4 r;
    r.m[0]  = 1.0f / (aspect * tanHalf);
    r.m[5]  = 1.0f / tanHalf;
    // Metal RH: z_ndc ∈ [0,1], w_clip = -z_view
    r.m[10] = farZ / (nearZ - farZ);     // = -farZ / (farZ - nearZ)
    r.m[11] = -1.0f;
    r.m[14] = nearZ * farZ / (nearZ - farZ);  // = -nearZ * farZ / (farZ - nearZ)
    return r;
}

Mat4 Mat4LookAt(float eyeX, float eyeY, float eyeZ,
                float targetX, float targetY, float targetZ,
                float upX, float upY, float upZ)
{
    // 前向向量 (eye → target)
    float fx = targetX - eyeX, fy = targetY - eyeY, fz = targetZ - eyeZ;
    float flen = std::sqrt(fx*fx + fy*fy + fz*fz);
    fx /= flen; fy /= flen; fz /= flen;

    // 右向量 = cross(前向, 上)
    float sx = fy * upZ - fz * upY;
    float sy = fz * upX - fx * upZ;
    float sz = fx * upY - fy * upX;
    float slen = std::sqrt(sx*sx + sy*sy + sz*sz);
    sx /= slen; sy /= slen; sz /= slen;

    // 上向量 = cross(右, 前向)
    float ux = sy * fz - sz * fy;
    float uy = sz * fx - sx * fz;
    float uz = sx * fy - sy * fx;

    Mat4 r;
    r.m[0] = sx;  r.m[4] = sy;  r.m[8]  = sz;  r.m[12] = -(sx*eyeX + sy*eyeY + sz*eyeZ);
    r.m[1] = ux;  r.m[5] = uy;  r.m[9]  = uz;  r.m[13] = -(ux*eyeX + uy*eyeY + uz*eyeZ);
    r.m[2] = -fx; r.m[6] = -fy; r.m[10] = -fz; r.m[14] =  (fx*eyeX + fy*eyeY + fz*eyeZ);
    r.m[3] = 0;   r.m[7] = 0;   r.m[11] = 0;   r.m[15] = 1.0f;
    return r;
}

Mat4 Mat4RotateY(float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    Mat4 r;
    r.m[0] = c;  r.m[4] = 0;  r.m[8]  = s;  r.m[12] = 0;
    r.m[1] = 0;  r.m[5] = 1;  r.m[9]  = 0;  r.m[13] = 0;
    r.m[2] = -s; r.m[6] = 0;  r.m[10] = c;  r.m[14] = 0;
    r.m[3] = 0;  r.m[7] = 0;  r.m[11] = 0;  r.m[15] = 1;
    return r;
}

Mat4 Mat4RotateX(float angle)
{
    float c = std::cos(angle);
    float s = std::sin(angle);
    Mat4 r;
    r.m[0] = 1;  r.m[4] = 0;   r.m[8]  = 0;  r.m[12] = 0;
    r.m[1] = 0;  r.m[5] = c;   r.m[9]  = -s; r.m[13] = 0;
    r.m[2] = 0;  r.m[6] = s;   r.m[10] = c;  r.m[14] = 0;
    r.m[3] = 0;  r.m[7] = 0;   r.m[11] = 0;  r.m[15] = 1;
    return r;
}

// ============================================================================
// Uniform buffer 数据（与 MSL 中的 packed_float3 布局对齐）
// ============================================================================
struct alignas(16) UniformData
{
    float mvpMatrix[16];   // 64 bytes, 列主序
    float lightPos[3];     // 12 bytes @ offset 64
    float _pad0;           // 4 bytes padding → offset 80
    float cameraPos[3];    // 12 bytes @ offset 80
    float _pad1;           // 4 bytes padding → offset 92
    // sizeof = 96, 对齐 = 16
};
static_assert(sizeof(UniformData) == 96, "UniformData 大小必须为 96 字节");

// ============================================================================
// 立方体顶点数据（36 顶点，6 面 × 2 三角 × 3 顶点）
// 每顶点: position(float3) + normal(float3), stride = 24 bytes
// ============================================================================
struct Vertex
{
    float position[3];
    float normal[3];
};

constexpr Vertex kCubeVertices[] = {
    // 前面 (z = 0.5), normal (0,0,1)
    {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}},
    {{ 0.5f, -0.5f,  0.5f}, { 0, 0, 1}},
    {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}},
    {{-0.5f, -0.5f,  0.5f}, { 0, 0, 1}},
    {{ 0.5f,  0.5f,  0.5f}, { 0, 0, 1}},
    {{-0.5f,  0.5f,  0.5f}, { 0, 0, 1}},

    // 后面 (z = -0.5), normal (0,0,-1)
    {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}},
    {{-0.5f, -0.5f, -0.5f}, { 0, 0,-1}},
    {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}},
    {{ 0.5f, -0.5f, -0.5f}, { 0, 0,-1}},
    {{-0.5f,  0.5f, -0.5f}, { 0, 0,-1}},
    {{ 0.5f,  0.5f, -0.5f}, { 0, 0,-1}},

    // 右面 (x = 0.5), normal (1,0,0)
    {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}},
    {{ 0.5f, -0.5f,  0.5f}, { 1, 0, 0}},
    {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}},
    {{ 0.5f, -0.5f, -0.5f}, { 1, 0, 0}},
    {{ 0.5f,  0.5f,  0.5f}, { 1, 0, 0}},
    {{ 0.5f,  0.5f, -0.5f}, { 1, 0, 0}},

    // 左面 (x = -0.5), normal (-1,0,0)
    {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}},
    {{-0.5f, -0.5f, -0.5f}, {-1, 0, 0}},
    {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}},
    {{-0.5f, -0.5f,  0.5f}, {-1, 0, 0}},
    {{-0.5f,  0.5f, -0.5f}, {-1, 0, 0}},
    {{-0.5f,  0.5f,  0.5f}, {-1, 0, 0}},

    // 上面 (y = 0.5), normal (0,1,0)
    {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}},
    {{ 0.5f,  0.5f,  0.5f}, { 0, 1, 0}},
    {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}},
    {{-0.5f,  0.5f,  0.5f}, { 0, 1, 0}},
    {{ 0.5f,  0.5f, -0.5f}, { 0, 1, 0}},
    {{-0.5f,  0.5f, -0.5f}, { 0, 1, 0}},

    // 下面 (y = -0.5), normal (0,-1,0)
    {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}},
    {{ 0.5f, -0.5f, -0.5f}, { 0,-1, 0}},
    {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}},
    {{-0.5f, -0.5f, -0.5f}, { 0,-1, 0}},
    {{ 0.5f, -0.5f,  0.5f}, { 0,-1, 0}},
    {{-0.5f, -0.5f,  0.5f}, { 0,-1, 0}},
};
constexpr std::uint32_t kNumCubeVertices = 36;

// ============================================================================
// 手写 MSL 着色器源代码
// ============================================================================
const char* kShaderSource = R"(
#include <metal_stdlib>
using namespace metal;

struct UniformData {
    float4x4 mvpMatrix;
    packed_float3 lightPos;
    packed_float3 cameraPos;
};

struct VertexOut {
    float4 position [[position]];
    float3 worldPos;
    float3 worldNormal;
};

vertex VertexOut vertexMain(uint vertexId [[vertex_id]],
                            constant packed_float3* positions [[buffer(0)]],
                            constant packed_float3* normals [[buffer(1)]],
                            constant UniformData& uniforms [[buffer(2)]])
{
    packed_float3 pos = positions[vertexId];
    packed_float3 norm = normals[vertexId];

    VertexOut out;
    out.position = uniforms.mvpMatrix * float4(pos, 1.0);
    out.worldPos = pos;
    out.worldNormal = norm;
    return out;
}

fragment float4 fragmentMain(VertexOut in [[stage_in]],
                              constant UniformData& uniforms [[buffer(0)]])
{
    float3 N = normalize(in.worldNormal);
    float3 L = normalize(uniforms.lightPos - in.worldPos);
    float3 V = normalize(uniforms.cameraPos - in.worldPos);
    float3 R = reflect(-L, N);

    float3 ambient  = float3(0.18, 0.18, 0.20);
    float3 diffuse  = float3(0.8, 0.6, 0.4) * max(dot(N, L), 0.0);
    float3 specular = float3(1.0, 1.0, 1.0) * pow(max(dot(R, V), 0.0), 48.0);

    return float4(ambient + diffuse + specular, 1.0);
}
)";

// ============================================================================
// 主函数
// ============================================================================
} // anonymous namespace

int main()
{
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    // ---- 1. 设备与命令队列 ----
    MTL::Device* device = MTL::CreateSystemDefaultDevice();
    if (!device)
    {
        std::cerr << "无法创建 MTLDevice。\n";
        pool->drain();
        return 1;
    }
    std::cout << "设备: " << device->name()->utf8String() << "\n";

    MTL::CommandQueue* commandQueue = device->newCommandQueue();
    if (!commandQueue)
    {
        std::cerr << "无法创建 MTLCommandQueue。\n";
        pool->drain();
        return 1;
    }

    // ---- 2. 编译内嵌 MSL 着色器 ----
    NS::Error* error = nullptr;
    NS::String* source = NS::String::string(kShaderSource, NS::UTF8StringEncoding);
    MTL::Library* library = device->newLibrary(source, nullptr, &error);
    if (!library)
    {
        std::cerr << "MSL 编译失败: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "MSL 着色器编译通过\n";

    MTL::Function* vertexFunction = library->newFunction(MTLSTR("vertexMain"));
    MTL::Function* fragmentFunction = library->newFunction(MTLSTR("fragmentMain"));
    if (!vertexFunction || !fragmentFunction)
    {
        std::cerr << "无法从 Library 获取着色器入口函数。\n";
        pool->drain();
        return 1;
    }

    // ---- 3. 创建顶点缓冲区 ----
    // 用两个独立的缓冲区：位置和法线
    constexpr std::uint32_t kVertexBytes = kNumCubeVertices * sizeof(float) * 3;

    // 位置缓冲区
    std::vector<float> positions(kNumCubeVertices * 3);
    for (std::uint32_t i = 0; i < kNumCubeVertices; ++i)
    {
        positions[i * 3 + 0] = kCubeVertices[i].position[0];
        positions[i * 3 + 1] = kCubeVertices[i].position[1];
        positions[i * 3 + 2] = kCubeVertices[i].position[2];
    }
    MTL::Buffer* vertexPosBuffer = device->newBuffer(
        positions.data(), kVertexBytes, MTL::ResourceStorageModeShared);
    if (!vertexPosBuffer)
    {
        std::cerr << "无法创建顶点位置缓冲区。\n";
        pool->drain();
        return 1;
    }

    // 法线缓冲区
    std::vector<float> normals(kNumCubeVertices * 3);
    for (std::uint32_t i = 0; i < kNumCubeVertices; ++i)
    {
        normals[i * 3 + 0] = kCubeVertices[i].normal[0];
        normals[i * 3 + 1] = kCubeVertices[i].normal[1];
        normals[i * 3 + 2] = kCubeVertices[i].normal[2];
    }
    MTL::Buffer* vertexNormalBuffer = device->newBuffer(
        normals.data(), kVertexBytes, MTL::ResourceStorageModeShared);
    if (!vertexNormalBuffer)
    {
        std::cerr << "无法创建顶点法线缓冲区。\n";
        pool->drain();
        return 1;
    }

    // ---- 4. 创建 Uniform Buffer ----
    UniformData uniformData;
    // 计算 MVP 矩阵
    Mat4 model = Mat4Mul(Mat4RotateX(0.3f), Mat4RotateY(0.6f)); // 旋转立方体
    Mat4 view  = Mat4LookAt(2.2f, 1.6f, 2.2f, 0, 0, 0, 0, 1, 0);
    Mat4 proj  = Mat4Perspective(3.14159f * 0.45f, float(kWidth)/float(kHeight), 0.1f, 100.0f);
    Mat4 mvp  = Mat4Mul(Mat4Mul(proj, view), model);
    std::memcpy(uniformData.mvpMatrix, mvp.m, sizeof(mvp.m));

    // 调试：打印 MVP 矩阵和测试顶点
    std::cout << "MVP 矩阵:\n";
    for (int r = 0; r < 4; ++r)
    {
        std::cout << "  [";
        for (int c = 0; c < 4; ++c)
            std::cout << mvp.m[c*4 + r] << (c < 3 ? ", " : "");
        std::cout << "]\n";
    }
    // 测试一个立方体顶点乘以 MVP
    float testVertex[4] = {0.5f, 0.5f, 0.5f, 1.0f};
    float result[4] = {0};
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c)
            result[r] += mvp.m[c*4 + r] * testVertex[c];
    std::cout << "测试顶点 (0.5,0.5,0.5) → clip ("
              << result[0] << ", " << result[1] << ", " << result[2] << ", " << result[3] << ")\n";
    if (result[3] != 0)
        std::cout << "  归一化 NDC: (" << result[0]/result[3] << ", " << result[1]/result[3] << ", " << result[2]/result[3] << ")\n";

    uniformData.lightPos[0] = 2.5f;
    uniformData.lightPos[1] = 3.0f;
    uniformData.lightPos[2] = 2.0f;
    uniformData._pad0 = 0;

    uniformData.cameraPos[0] = 2.2f;
    uniformData.cameraPos[1] = 1.6f;
    uniformData.cameraPos[2] = 2.2f;
    uniformData._pad1 = 0;

    MTL::Buffer* uniformBuffer = device->newBuffer(
        &uniformData, sizeof(UniformData), MTL::ResourceStorageModeShared);
    if (!uniformBuffer)
    {
        std::cerr << "无法创建 Uniform Buffer。\n";
        pool->drain();
        return 1;
    }
    std::cout << "Uniform Buffer 已创建, 大小=" << sizeof(UniformData) << " 字节\n";
    std::cout << "光源位置: (" << uniformData.lightPos[0] << ", "
              << uniformData.lightPos[1] << ", " << uniformData.lightPos[2] << ")\n";
    std::cout << "相机位置: (" << uniformData.cameraPos[0] << ", "
              << uniformData.cameraPos[1] << ", " << uniformData.cameraPos[2] << ")\n";

    // ---- 5. 创建深度纹理 ----
    MTL::TextureDescriptor* depthTexDesc = MTL::TextureDescriptor::alloc()->init();
    depthTexDesc->setTextureType(MTL::TextureType2D);
    depthTexDesc->setPixelFormat(MTL::PixelFormatDepth32Float);
    depthTexDesc->setWidth(kWidth);
    depthTexDesc->setHeight(kHeight);
    depthTexDesc->setUsage(MTL::TextureUsageRenderTarget);
    depthTexDesc->setStorageMode(MTL::StorageModePrivate);
    MTL::Texture* depthTexture = device->newTexture(depthTexDesc);
    if (!depthTexture)
    {
        std::cerr << "无法创建深度纹理。\n";
        pool->drain();
        return 1;
    }
    std::cout << "深度纹理已创建: " << kWidth << "x" << kHeight
              << ", format=Depth32Float\n";

    // ---- 6. 创建 DepthStencilState ----
    MTL::DepthStencilDescriptor* depthStencilDesc =
        MTL::DepthStencilDescriptor::alloc()->init();
    depthStencilDesc->setDepthCompareFunction(MTL::CompareFunctionLess);
    depthStencilDesc->setDepthWriteEnabled(true);
    MTL::DepthStencilState* depthStencilState =
        device->newDepthStencilState(depthStencilDesc);
    if (!depthStencilState)
    {
        std::cerr << "无法创建 DepthStencilState。\n";
        pool->drain();
        return 1;
    }
    std::cout << "DepthStencilState 已创建: CompareLess, WriteEnabled\n";

    // ---- 7. 创建离屏颜色纹理 ----
    MTL::TextureDescriptor* colorTexDesc =
        MTL::TextureDescriptor::texture2DDescriptor(
            MTL::PixelFormatBGRA8Unorm, kWidth, kHeight, false);
    colorTexDesc->setUsage(MTL::TextureUsageRenderTarget | MTL::TextureUsageShaderRead);
    colorTexDesc->setStorageMode(MTL::StorageModeShared);
    MTL::Texture* colorTexture = device->newTexture(colorTexDesc);
    if (!colorTexture)
    {
        std::cerr << "无法创建颜色纹理。\n";
        pool->drain();
        return 1;
    }

    // ---- 8. 创建渲染管线 ----
    MTL::RenderPipelineDescriptor* pipelineDesc =
        MTL::RenderPipelineDescriptor::alloc()->init();
    pipelineDesc->setVertexFunction(vertexFunction);
    pipelineDesc->setFragmentFunction(fragmentFunction);
    pipelineDesc->colorAttachments()->object(0)->setPixelFormat(MTL::PixelFormatBGRA8Unorm);
    pipelineDesc->setDepthAttachmentPixelFormat(MTL::PixelFormatDepth32Float);

    MTL::RenderPipelineState* pipelineState =
        device->newRenderPipelineState(pipelineDesc, &error);
    if (!pipelineState)
    {
        std::cerr << "无法创建 RenderPipelineState: " << ErrorToString(error) << "\n";
        pool->drain();
        return 1;
    }
    std::cout << "RenderPipelineState 已创建 (depth format=Depth32Float)\n";

    // ---- 9. 渲染 ----
    MTL::RenderPassDescriptor* passDesc = MTL::RenderPassDescriptor::alloc()->init();

    // 颜色 attachment
    MTL::RenderPassColorAttachmentDescriptor* colorAttachment =
        passDesc->colorAttachments()->object(0);
    colorAttachment->setTexture(colorTexture);
    colorAttachment->setLoadAction(MTL::LoadActionClear);
    colorAttachment->setStoreAction(MTL::StoreActionStore);
    colorAttachment->setClearColor(MTL::ClearColor(0.06, 0.08, 0.12, 1.0));

    // 深度 attachment
    MTL::RenderPassDepthAttachmentDescriptor* depthAttachment =
        passDesc->depthAttachment();
    depthAttachment->setTexture(depthTexture);
    depthAttachment->setLoadAction(MTL::LoadActionClear);
    depthAttachment->setStoreAction(MTL::StoreActionDontCare);
    depthAttachment->setClearDepth(1.0);

    MTL::CommandBuffer* commandBuffer = commandQueue->commandBuffer();
    MTL::RenderCommandEncoder* encoder =
        commandBuffer->renderCommandEncoder(passDesc);

    encoder->setRenderPipelineState(pipelineState);

    // 设置深度测试
    encoder->setDepthStencilState(depthStencilState);

    // 设置背面剔除
    encoder->setCullMode(MTL::CullModeBack);
    encoder->setFrontFacingWinding(MTL::WindingCounterClockwise);

    // 绑定顶点缓冲区（位置 @ buffer 0, 法线 @ buffer 1）
    encoder->setVertexBuffer(vertexPosBuffer,   0, 0);
    encoder->setVertexBuffer(vertexNormalBuffer, 0, 1);

    // 绑定 Uniform Buffer（VS @ buffer 2, FS @ buffer 0）
    encoder->setVertexBuffer(uniformBuffer,   0, 2);
    encoder->setFragmentBuffer(uniformBuffer,  0, 0);

    // 绘制 36 个顶点（立方体）
    encoder->drawPrimitives(MTL::PrimitiveTypeTriangle,
                            NS::UInteger(0), NS::UInteger(kNumCubeVertices));
    encoder->endEncoding();
    commandBuffer->commit();
    commandBuffer->waitUntilCompleted();

    std::cout << "绘制完成: " << kNumCubeVertices << " 顶点\n";

    // ---- 10. 读取颜色缓冲到 PPM ----
    std::vector<Pixel> pixels(kWidth * kHeight);
    colorTexture->getBytes(
        pixels.data(),
        static_cast<NS::UInteger>(kWidth * sizeof(Pixel)),
        MTL::Region::Make2D(0, 0, kWidth, kHeight),
        0);

    std::filesystem::create_directories("out");
    const std::filesystem::path outputPath =
        std::filesystem::path("out") / "basic_lighting.ppm";
    if (!WritePpm(outputPath, pixels, kWidth, kHeight))
    {
        std::cerr << "无法写出渲染结果: " << outputPath << "\n";
        pool->drain();
        return 1;
    }

    std::cout << "D4 离屏渲染完成: " << outputPath << "\n";

    pool->drain();
    return 0;
}
