// test_texture.cpp — MetalTexture 单元测试（Catch2）
//
// 测试覆盖：
//   1. metal_create_texture 创建 RGBA8 颜色纹理
//   2. metal_texture_get_info 返回正确信息
//   3. metal_pixel_format_get_info 查询格式信息
//   4. 上传 → 回读闭环（RGBA8 像素数据）
//   5. metal_create_texture 创建深度纹理
//   6. 深度纹理信息查询
//   7. NULL 参数返回 INVALID_ARGUMENT
//   8. 纹理尺寸超出限制
//   9. 多种常用格式创建
//   10. mipmap 纹理创建
//   11. 像素格式名称字符串正确
//   12. 创建-释放循环无泄漏
//   13. metal_create_texture 创建 cube 纹理
//   14. 上传非法 mip level 返回错误

#include <catch2/catch_test_macros.hpp>

#include "metal_bridge.h"
#include "metal_limits.h"

#include <cstring>
#include <cstdint>
#include <string>

// ════════════════════════════════════════════════════════════════════
// 辅助：设备和纹理的 RAII 包装
// ════════════════════════════════════════════════════════════════════

struct DeviceGuard
{
    metal_device* dev = nullptr;

    DeviceGuard()
    {
        metal_result result = metal_create_device(&dev);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(dev != nullptr);
    }

    ~DeviceGuard()
    {
        if (dev != nullptr)
        {
            metal_release(dev);
            dev = nullptr;
        }
    }

    DeviceGuard(const DeviceGuard&) = delete;
    DeviceGuard& operator=(const DeviceGuard&) = delete;
};

// ════════════════════════════════════════════════════════════════════
// 测试用例
// ════════════════════════════════════════════════════════════════════

TEST_CASE("metal_create_texture 创建 RGBA8 颜色纹理", "[texture][create]")
{
    DeviceGuard dev_guard;

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        64, 64, 1,  // width, height, depth
        1,          // levels
        1,          // samples
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);
    metal_release(tex);
}

TEST_CASE("metal_texture_get_info 返回正确信息", "[texture][info]")
{
    DeviceGuard dev_guard;

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_BGRA8_UNORM,
        128, 64, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);

    metal_texture_info info;
    std::memset(&info, 0xFF, sizeof(info));

    result = metal_texture_get_info(tex, &info);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(info.width == 128);
    REQUIRE(info.height == 64);
    REQUIRE(info.depth == 1);
    REQUIRE(info.levels == 1);
    REQUIRE(info.samples == 1);
    REQUIRE(info.type == METAL_TEXTURE_TYPE_2D);
    REQUIRE(info.pixel_format == METAL_PIXEL_FORMAT_BGRA8_UNORM);
    REQUIRE(info.storage_mode == METAL_STORAGE_MODE_SHARED);

    metal_release(tex);
}

TEST_CASE("metal_pixel_format_get_info 查询格式信息", "[texture][format]")
{
    DeviceGuard dev_guard;

    // 查询 RGBA8Unorm
    metal_pixel_format_info info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_RGBA8_UNORM);
    REQUIRE(info.bytes_per_pixel == 4);
    REQUIRE(info.block_width == 1);
    REQUIRE(info.block_height == 1);
    REQUIRE(info.is_depth == false);
    REQUIRE(info.is_compressed == false);
    REQUIRE(info.is_srgb == false);
    REQUIRE(std::strlen(info.name) > 0);

    // 查询 D32Float
    info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_D32_FLOAT);
    REQUIRE(info.bytes_per_pixel == 4);
    REQUIRE(info.is_depth == true);
    REQUIRE(info.is_compressed == false);

    // 查询 BC1 压缩格式
    info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_BC1_RGBA);
    REQUIRE(info.bytes_per_pixel == 0);
    REQUIRE(info.block_width == 4);
    REQUIRE(info.block_height == 4);
    REQUIRE(info.is_compressed == true);

    // 查询无效格式返回全零（bytes_per_pixel=0, block=1x1）
    info = metal_pixel_format_get_info(static_cast<metal_pixel_format>(999));
    REQUIRE(info.bytes_per_pixel == 0);
    REQUIRE(info.block_width == 1);  // 默认值
    REQUIRE(info.block_height == 1); // 默认值

    // 查询 sRGB 格式
    info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_RGBA8_SRGB);
    REQUIRE(info.is_srgb == true);

    info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_BGRA8_SRGB);
    REQUIRE(info.is_srgb == true);
}

TEST_CASE("纹理上传→回读闭环（RGBA8 像素数据）", "[texture][upload][readback]")
{
    DeviceGuard dev_guard;

    constexpr uint32_t kTexWidth = 16;
    constexpr uint32_t kTexHeight = 16;

    // 创建颜色纹理
    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);

    // 准备像素数据：每个像素 (r, g, b, a) = (x*16, y*16, 128, 255)
    // 使用可预测的模式，这样回读后可以精确验证每个像素
    constexpr uint32_t kBytesPerRow = kTexWidth * 4;
    constexpr uint32_t kDataSize = kTexHeight * kBytesPerRow;
    uint8_t src_data[kDataSize];
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            src_data[idx + 0] = static_cast<uint8_t>(x * 16);    // R: 0, 16, 32, ... 240
            src_data[idx + 1] = static_cast<uint8_t>(y * 16);    // G: 0, 16, 32, ... 240
            src_data[idx + 2] = 128;                              // B: 固定 128
            src_data[idx + 3] = 255;                              // A: 固定 255
        }
    }

    // 创建上传用的 Shared 缓冲区
    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, src_data, kDataSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(upload_buf != nullptr);

    // 上传到纹理
    result = metal_texture_upload(
        tex, upload_buf,
        0,          // buffer_offset
        0,          // layer
        0,          // level
        0, 0, 0,    // region_x, y, z
        kTexWidth, kTexHeight,
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 创建回读用的 Shared 缓冲区
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kDataSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(readback_buf != nullptr);

    // 从纹理回读到缓冲区
    result = metal_texture_readback(
        tex, readback_buf,
        0,          // buffer_offset
        0,          // layer
        0,          // level
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 验证回读数据 — 精确验证 8 个关键像素的 RGBA 值
    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(mapped != nullptr);

    const uint8_t* readback_data = static_cast<const uint8_t*>(mapped);

    // 结构：验证特定位置的已知像素值
    // (0,0) → R=0, G=0, B=128, A=255
    // (1,1) → R=16, G=16, B=128, A=255
    // (7,7) → R=112, G=112, B=128, A=255
    // (15,15) → R=240, G=240, B=128, A=255
    // (0,15) → R=0, G=240, B=128, A=255
    // (15,0) → R=240, G=0, B=128, A=255
    // (3,8)  → R=48, G=128, B=128, A=255
    // (10,5) → R=160, G=80, B=128, A=255

    struct PixelCheck { uint32_t x, y; uint8_t r, g, b, a; };
    const PixelCheck checks[] = {
        { 0,  0,   0,   0, 128, 255},
        { 1,  1,  16,  16, 128, 255},
        { 7,  7, 112, 112, 128, 255},
        {15, 15, 240, 240, 128, 255},
        { 0, 15,   0, 240, 128, 255},
        {15,  0, 240,   0, 128, 255},
        { 3,  8,  48, 128, 128, 255},
        {10,  5, 160,  80, 128, 255},
    };

    for (const auto& check : checks)
    {
        uint32_t idx = (check.y * kTexWidth + check.x) * 4;
        INFO("像素 (" << check.x << "," << check.y << "): 期望 "
             << (int)check.r << "," << (int)check.g << "," << (int)check.b << "," << (int)check.a);
        REQUIRE(readback_data[idx + 0] == check.r);
        REQUIRE(readback_data[idx + 1] == check.g);
        REQUIRE(readback_data[idx + 2] == check.b);
        REQUIRE(readback_data[idx + 3] == check.a);
    }

    // 额外验证：所有像素的 B 通道固定为 128，A 通道固定为 255
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            REQUIRE(readback_data[idx + 2] == 128); // B = 128
            REQUIRE(readback_data[idx + 3] == 255); // A = 255
        }
    }

    metal_release(readback_buf);
    metal_release(upload_buf);
    metal_release(tex);
}

TEST_CASE("纹理上传 buffer_offset 非零位移", "[texture][upload][offset]")
{
    DeviceGuard dev_guard;

    constexpr uint32_t kTexWidth = 8;
    constexpr uint32_t kTexHeight = 8;
    constexpr uint32_t kBytesPerRow = kTexWidth * 4;
    constexpr uint32_t kTexDataSize = kTexHeight * kBytesPerRow;
    // 缓冲区总大小比纹理数据大（在前面保留 buffer_offset 字节）
    constexpr uint32_t kBufferOffset = 256;
    constexpr uint32_t kBufferSize = kBufferOffset + kTexDataSize;

    // 创建颜色纹理
    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);

    // 准备像素数据：每个像素 (r, g, b, a) = (x*32, y*32, 64, 255)
    uint8_t src_data[kBufferSize];
    // 填充偏移区域为已知标记值
    std::memset(src_data, 0xCD, kBufferOffset);
    // 填充纹理数据
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = kBufferOffset + (y * kTexWidth + x) * 4;
            src_data[idx + 0] = static_cast<uint8_t>(x * 32);    // R: 0, 32, 64, ... 224
            src_data[idx + 1] = static_cast<uint8_t>(y * 32);    // G: 0, 32, 64, ... 224
            src_data[idx + 2] = 64;                               // B: 固定 64
            src_data[idx + 3] = 255;                              // A: 固定 255
        }
    }

    // 创建带完整数据的 Shared 缓冲区（含偏移区域）
    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, src_data, kBufferSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(upload_buf != nullptr);

    // 使用非零 buffer_offset 上传
    result = metal_texture_upload(
        tex, upload_buf,
        kBufferOffset,  // buffer_offset = 256 字节
        0,              // layer
        0,              // level
        0, 0, 0,        // region
        kTexWidth, kTexHeight,
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 创建回读用的 Shared 缓冲区
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kTexDataSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(readback_buf != nullptr);

    // 从纹理回读（无偏移）
    result = metal_texture_readback(
        tex, readback_buf,
        0,          // buffer_offset = 0
        0,          // layer
        0,          // level
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 验证回读数据 — 精确验证
    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(mapped != nullptr);

    const uint8_t* readback_data = static_cast<const uint8_t*>(mapped);

    // 验证几个关键像素
    struct PixelCheck { uint32_t x, y; uint8_t r, g, b, a; };
    const PixelCheck checks[] = {
        { 0, 0,   0,   0,  64, 255},
        { 1, 1,  32,  32,  64, 255},
        { 4, 4, 128, 128,  64, 255},
        { 7, 7, 224, 224,  64, 255},
        { 0, 7,   0, 224,  64, 255},
        { 7, 0, 224,   0,  64, 255},
    };

    for (const auto& check : checks)
    {
        uint32_t idx = (check.y * kTexWidth + check.x) * 4;
        INFO("buffer_offset 测试像素 (" << check.x << "," << check.y << ")");
        REQUIRE(readback_data[idx + 0] == check.r);
        REQUIRE(readback_data[idx + 1] == check.g);
        REQUIRE(readback_data[idx + 2] == check.b);
        REQUIRE(readback_data[idx + 3] == check.a);
    }

    // 验证所有像素 B=64, A=255
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            REQUIRE(readback_data[idx + 2] == 64);
            REQUIRE(readback_data[idx + 3] == 255);
        }
    }

    metal_release(readback_buf);
    metal_release(upload_buf);
    metal_release(tex);
}

TEST_CASE("metal_create_texture 创建深度纹理", "[texture][depth]")
{
    DeviceGuard dev_guard;

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_D32_FLOAT,
        256, 256, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_PRIVATE,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);

    metal_texture_info info;
    result = metal_texture_get_info(tex, &info);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(info.pixel_format == METAL_PIXEL_FORMAT_D32_FLOAT);
    REQUIRE(info.width == 256);
    REQUIRE(info.height == 256);

    metal_release(tex);
}

TEST_CASE("NULL 参数返回 INVALID_ARGUMENT", "[texture][error]")
{
    DeviceGuard dev_guard;
    metal_texture_info info;

    // create_texture NULL 参数
    REQUIRE(metal_create_texture(
        nullptr, METAL_PIXEL_FORMAT_RGBA8_UNORM,
        64, 64, 1, 1, 1, METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED, nullptr) == METAL_RESULT_INVALID_ARGUMENT);

    // get_info NULL 参数
    REQUIRE(metal_texture_get_info(nullptr, &info) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_texture_get_info(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);

    // upload NULL 参数
    REQUIRE(metal_texture_upload(
        nullptr, nullptr, 0, 0, 0, 0, 0, 0, 1, 1, 4) == METAL_RESULT_INVALID_ARGUMENT);

    // readback NULL 参数
    REQUIRE(metal_texture_readback(
        nullptr, nullptr, 0, 0, 0, 0) == METAL_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("零尺寸纹理返回 INVALID_ARGUMENT", "[texture][error][sizes]")
{
    DeviceGuard dev_guard;

    metal_texture* tex = nullptr;

    // width=0
    REQUIRE(metal_create_texture(
        dev_guard.dev, METAL_PIXEL_FORMAT_RGBA8_UNORM,
        0, 64, 1, 1, 1, METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED, &tex) == METAL_RESULT_INVALID_ARGUMENT);

    // height=0
    REQUIRE(metal_create_texture(
        dev_guard.dev, METAL_PIXEL_FORMAT_RGBA8_UNORM,
        64, 0, 1, 1, 1, METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED, &tex) == METAL_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("多种常用格式创建纹理", "[texture][formats]")
{
    DeviceGuard dev_guard;

    // 测试一组常用格式能否成功创建
    struct FormatTest
    {
        metal_pixel_format fmt;
        uint32_t bpp;
        bool is_depth;
    };

    const FormatTest test_formats[] = {
        {METAL_PIXEL_FORMAT_R8_UNORM,     1, false},
        {METAL_PIXEL_FORMAT_R16_FLOAT,    2, false},
        {METAL_PIXEL_FORMAT_RGBA8_UNORM,  4, false},
        {METAL_PIXEL_FORMAT_BGRA8_UNORM,  4, false},
        {METAL_PIXEL_FORMAT_RGBA16_FLOAT, 8, false},
        {METAL_PIXEL_FORMAT_RGBA32_FLOAT, 16, false},
        {METAL_PIXEL_FORMAT_R32_FLOAT,    4, false},
        {METAL_PIXEL_FORMAT_R11G11B10_FLOAT, 4, false},
        {METAL_PIXEL_FORMAT_D32_FLOAT,    4, true},
    };

    for (const auto& ft : test_formats)
    {
        metal_texture* tex = nullptr;
        metal_storage_mode storage = ft.is_depth ? METAL_STORAGE_MODE_PRIVATE
                                                  : METAL_STORAGE_MODE_SHARED;

        metal_result result = metal_create_texture(
            dev_guard.dev, ft.fmt,
            32, 32, 1, 1, 1,
            METAL_TEXTURE_TYPE_2D,
            METAL_TEXTURE_USAGE_SHADER_READ,
            storage,
            &tex);

        // 某些格式在特定硬件上可能不受支持，用 CHECK 而非 REQUIRE
        if (result == METAL_RESULT_OK)
        {
            CHECK(tex != nullptr);

            metal_pixel_format_info finfo = metal_pixel_format_get_info(ft.fmt);
            CHECK(finfo.bytes_per_pixel == ft.bpp);
            CHECK(finfo.is_depth == ft.is_depth);

            metal_release(tex);
        }
    }
}

TEST_CASE("mipmap 纹理创建", "[texture][mipmap]")
{
    DeviceGuard dev_guard;

    constexpr uint32_t kLevels = 4; // 从 16x16 到 2x2

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        16, 16, 1, kLevels, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);

    metal_texture_info info;
    result = metal_texture_get_info(tex, &info);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(info.levels == kLevels);

    metal_release(tex);
}

TEST_CASE("像素格式名称字符串正确", "[texture][format][name]")
{
    // 验证几个关键格式的名称字符串
    metal_pixel_format_info info;

    info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_RGBA8_UNORM);
    CHECK(std::string(info.name) == "RGBA8Unorm");

    info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_D32_FLOAT);
    CHECK(std::string(info.name) == "Depth32Float");

    info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_BC3_RGBA);
    CHECK(std::string(info.name) == "BC3_RGBA");

    info = metal_pixel_format_get_info(METAL_PIXEL_FORMAT_ASTC_4x4_LDR);
    CHECK(std::string(info.name) == "ASTC_4x4_LDR");

    // 无效格式名称应为空字符串
    info = metal_pixel_format_get_info(static_cast<metal_pixel_format>(999));
    CHECK(std::string(info.name) == "");
}

TEST_CASE("创建-释放循环无泄漏", "[texture][release]")
{
    DeviceGuard dev_guard;

    for (int i = 0; i < 5; ++i)
    {
        metal_texture* tex = nullptr;
        metal_result result = metal_create_texture(
            dev_guard.dev,
            METAL_PIXEL_FORMAT_RGBA8_UNORM,
            32, 32, 1, 1, 1,
            METAL_TEXTURE_TYPE_2D,
            METAL_TEXTURE_USAGE_SHADER_READ,
            METAL_STORAGE_MODE_SHARED,
            &tex);

        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(tex != nullptr);
        metal_release(tex);
    }
}

TEST_CASE("metal_create_texture 创建 cube 纹理", "[texture][cube]")
{
    DeviceGuard dev_guard;

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        32, 32, 1, 1, 1,
        METAL_TEXTURE_TYPE_CUBE,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    // Cube 纹理可能受硬件限制，允许 UNSUPPORTED
    if (result == METAL_RESULT_OK)
    {
        REQUIRE(tex != nullptr);

        metal_texture_info info;
        result = metal_texture_get_info(tex, &info);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(info.type == METAL_TEXTURE_TYPE_CUBE);

        metal_release(tex);
    }
}

TEST_CASE("上传非法 mip level 返回错误", "[texture][upload][error]")
{
    DeviceGuard dev_guard;

    // 创建只有 1 个 mip level 的纹理
    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        16, 16, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);

    // 创建上传缓冲区
    constexpr uint32_t kBytesPerRow = 16 * 4;
    constexpr uint32_t kDataSize = 16 * kBytesPerRow;
    metal_buffer* buf = nullptr;
    result = metal_create_buffer(dev_guard.dev, kDataSize, METAL_STORAGE_MODE_SHARED, &buf);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(buf != nullptr);

    // level=1 但纹理只有 1 个 level（索引 0）→ 非法
    result = metal_texture_upload(
        tex, buf, 0, 0, 1,  // level=1 超出范围
        0, 0, 0, 16, 16, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_INVALID_ARGUMENT);

    metal_release(buf);
    metal_release(tex);
}

TEST_CASE("mip level 上传与回读", "[texture][mipmap][upload][readback]")
{
    DeviceGuard dev_guard;

    // 创建 2 级 mipmap 纹理 (8x8 → 4x4)
    constexpr uint32_t kBaseWidth = 8;
    constexpr uint32_t kBaseHeight = 8;
    constexpr uint32_t kLevels = 2;

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kBaseWidth, kBaseHeight, 1, kLevels, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);

    // 准备 level 0 数据 (8x8): 红色调 (255, 0, 128, 255)
    constexpr uint32_t kLevel0RowBytes = kBaseWidth * 4;
    constexpr uint32_t kLevel0Size = kBaseHeight * kLevel0RowBytes;
    uint8_t level0_data[kLevel0Size];
    for (uint32_t i = 0; i < kLevel0Size; i += 4)
    {
        level0_data[i + 0] = 255;   // R
        level0_data[i + 1] = 0;     // G
        level0_data[i + 2] = 128;   // B
        level0_data[i + 3] = 255;   // A
    }

    // 准备 level 1 数据 (4x4): 绿色调 (0, 255, 64, 255)
    constexpr uint32_t kLevel1Width = 4;
    constexpr uint32_t kLevel1Height = 4;
    constexpr uint32_t kLevel1RowBytes = kLevel1Width * 4;
    constexpr uint32_t kLevel1Size = kLevel1Height * kLevel1RowBytes;
    uint8_t level1_data[kLevel1Size];
    for (uint32_t i = 0; i < kLevel1Size; i += 4)
    {
        level1_data[i + 0] = 0;     // R
        level1_data[i + 1] = 255;   // G
        level1_data[i + 2] = 64;    // B
        level1_data[i + 3] = 255;   // A
    }

    // 上传 level 0
    metal_buffer* level0_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, level0_data, kLevel0Size, METAL_STORAGE_MODE_SHARED, &level0_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        tex, level0_buf, 0, 0, 0,  // buffer_offset=0, layer=0, level=0
        0, 0, 0, kBaseWidth, kBaseHeight, kLevel0RowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    // 上传 level 1
    metal_buffer* level1_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, level1_data, kLevel1Size, METAL_STORAGE_MODE_SHARED, &level1_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        tex, level1_buf, 0, 0, 1,  // buffer_offset=0, layer=0, level=1
        0, 0, 0, kLevel1Width, kLevel1Height, kLevel1RowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    // 回读 level 0 并验证
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kLevel0Size, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        tex, readback_buf, 0, 0, 0, kLevel0RowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* readback = static_cast<const uint8_t*>(mapped);
    // 验证 level 0 为红色调
    REQUIRE(readback[0] == 255);
    REQUIRE(readback[1] == 0);
    REQUIRE(readback[2] == 128);
    REQUIRE(readback[3] == 255);
    metal_release(readback_buf);

    // 回读 level 1 并验证
    result = metal_create_buffer(
        dev_guard.dev, kLevel1Size, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        tex, readback_buf, 0, 0, 1, kLevel1RowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    readback = static_cast<const uint8_t*>(mapped);
    // 验证 level 1 为绿色调
    REQUIRE(readback[0] == 0);
    REQUIRE(readback[1] == 255);
    REQUIRE(readback[2] == 64);
    REQUIRE(readback[3] == 255);

    metal_release(readback_buf);
    metal_release(level1_buf);
    metal_release(level0_buf);
    metal_release(tex);
}

TEST_CASE("readback buffer_offset 非零位移", "[texture][readback][offset]")
{
    DeviceGuard dev_guard;

    constexpr uint32_t kTexWidth = 8;
    constexpr uint32_t kTexHeight = 8;
    constexpr uint32_t kBytesPerRow = kTexWidth * 4;
    constexpr uint32_t kTexDataSize = kTexHeight * kBytesPerRow;
    constexpr uint32_t kBufferOffset = 256;
    constexpr uint32_t kBufferSize = kBufferOffset + kTexDataSize;

    // 创建颜色纹理
    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(tex != nullptr);

    // 准备像素数据: 阶梯模式 (x*32, y*32, 96, 255)
    uint8_t src_data[kTexDataSize];
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            src_data[idx + 0] = static_cast<uint8_t>(x * 32);
            src_data[idx + 1] = static_cast<uint8_t>(y * 32);
            src_data[idx + 2] = 96;
            src_data[idx + 3] = 255;
        }
    }

    // 上传到纹理（偏移=0）
    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, src_data, kTexDataSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        tex, upload_buf, 0, 0, 0,
        0, 0, 0, kTexWidth, kTexHeight, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 创建带有偏移区域的大缓冲区
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kBufferSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    // 在偏移区域填入标记值，验证 readback 不会写入此处
    void* mapped = nullptr;
    metal_map_buffer(readback_buf, &mapped);
    std::memset(mapped, 0xCD, kBufferOffset);
    // 目标区域清零
    std::memset(static_cast<uint8_t*>(mapped) + kBufferOffset, 0, kTexDataSize);
    metal_unmap_buffer(readback_buf);

    // 使用非零 buffer_offset 回读
    result = metal_texture_readback(
        tex, readback_buf,
        kBufferOffset,  // buffer_offset = 256
        0,              // layer
        0,              // level
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 验证偏移区域未被覆盖
    metal_map_buffer(readback_buf, &mapped);
    const uint8_t* data = static_cast<const uint8_t*>(mapped);
    for (uint32_t i = 0; i < kBufferOffset; ++i)
    {
        REQUIRE(data[i] == 0xCD);  // 偏移区域应保持不变
    }

    // 验证纹理数据正确写入到偏移后的位置
    const uint8_t* tex_data = data + kBufferOffset;
    // 验证几个关键像素
    REQUIRE(tex_data[0] == 0);     // (0,0) R
    REQUIRE(tex_data[4] == 32);   // (1,0) R
    REQUIRE(tex_data[7] == 255);  // (1,0) A
    REQUIRE(tex_data[4*8*4 + 0] == 0);  // (0,1) R
    REQUIRE(tex_data[4*8*4 + 4] == 32); // (1,1) R

    metal_release(readback_buf);
    metal_release(upload_buf);
    metal_release(tex);
}

TEST_CASE("无效像素格式返回 INVALID_ARGUMENT", "[texture][format][error]")
{
    DeviceGuard dev_guard;

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        static_cast<metal_pixel_format>(999), // 无效格式
        64, 64, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    REQUIRE(result == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(tex == nullptr);
}

TEST_CASE("2D 纹理数组创建", "[texture][array]")
{
    DeviceGuard dev_guard;

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        32, 32, 4, 1, 1,  // depth=4 作为数组层数
        METAL_TEXTURE_TYPE_2D_ARRAY,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    // 某些硬件或配置可能不支持 array 纹理，允许 UNSUPPORTED
    if (result == METAL_RESULT_OK)
    {
        REQUIRE(tex != nullptr);

        metal_texture_info info;
        result = metal_texture_get_info(tex, &info);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(info.type == METAL_TEXTURE_TYPE_2D_ARRAY);
        REQUIRE(info.depth == 4);

        metal_release(tex);
    }
    else
    {
        REQUIRE(result == METAL_RESULT_UNSUPPORTED);
    }
}

TEST_CASE("3D 纹理创建", "[texture][3d]")
{
    DeviceGuard dev_guard;

    metal_texture* tex = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        16, 16, 8, 1, 1,  // depth=8
        METAL_TEXTURE_TYPE_3D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED,
        &tex);

    // 某些配置可能不支持 3D 纹理，允许 UNSUPPORTED
    if (result == METAL_RESULT_OK)
    {
        REQUIRE(tex != nullptr);

        metal_texture_info info;
        result = metal_texture_get_info(tex, &info);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(info.type == METAL_TEXTURE_TYPE_3D);
        REQUIRE(info.depth == 8);

        metal_release(tex);
    }
    else
    {
        REQUIRE(result == METAL_RESULT_UNSUPPORTED);
    }
}

// ════════════════════════════════════════════════════════════════════
// metal_create_texture_view 测试
// ════════════════════════════════════════════════════════════════════

TEST_CASE("metal_create_texture_view 创建 2D 全尺寸视图", "[texture][view][create]")
{
    DeviceGuard dev_guard;

    // 创建父纹理
    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        32, 32, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(parent != nullptr);

    // 创建全尺寸视图（相同格式、全层 0/1、全级 0/1）
    metal_texture* view = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1,   // first_layer=0, num_layers=1
        0, 1,   // first_level=0, num_levels=1
        &view);

    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view != nullptr);
    REQUIRE(view != parent);

    // 验证视图信息与父纹理一致
    metal_texture_info view_info;
    result = metal_texture_get_info(view, &view_info);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view_info.width == 32);
    REQUIRE(view_info.height == 32);
    REQUIRE(view_info.depth == 1);
    REQUIRE(view_info.levels == 1);
    REQUIRE(view_info.samples == 1);
    REQUIRE(view_info.type == METAL_TEXTURE_TYPE_2D);
    REQUIRE(view_info.pixel_format == METAL_PIXEL_FORMAT_RGBA8_UNORM);
    REQUIRE(view_info.storage_mode == METAL_STORAGE_MODE_SHARED);

    metal_release(view);
    metal_release(parent);
}

TEST_CASE("metal_create_texture_view NULL 参数返回 INVALID_ARGUMENT", "[texture][view][error]")
{
    DeviceGuard dev_guard;

    metal_texture* view = nullptr;

    // parent 为 NULL
    REQUIRE(metal_create_texture_view(
        nullptr, METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1, 0, 1, &view) == METAL_RESULT_INVALID_ARGUMENT);

    // num_layers 为 0
    metal_texture* parent = nullptr;
    metal_create_texture(
        dev_guard.dev, METAL_PIXEL_FORMAT_RGBA8_UNORM,
        16, 16, 1, 1, 1, METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED, &parent);

    REQUIRE(parent != nullptr);
    REQUIRE(metal_create_texture_view(
        parent, METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 0, 0, 1, &view) == METAL_RESULT_INVALID_ARGUMENT);

    // num_levels 为 0
    REQUIRE(metal_create_texture_view(
        parent, METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1, 0, 0, &view) == METAL_RESULT_INVALID_ARGUMENT);

    metal_release(parent);
}

TEST_CASE("metal_create_texture_view 无效格式返回 INVALID_ARGUMENT", "[texture][view][error]")
{
    DeviceGuard dev_guard;

    metal_texture* parent = nullptr;
    metal_create_texture(
        dev_guard.dev, METAL_PIXEL_FORMAT_RGBA8_UNORM,
        16, 16, 1, 1, 1, METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED, &parent);
    REQUIRE(parent != nullptr);

    metal_texture* view = nullptr;
    metal_result result = metal_create_texture_view(
        parent,
        static_cast<metal_pixel_format>(999),  // 无效格式
        METAL_TEXTURE_TYPE_2D,
        0, 1, 0, 1, &view);

    REQUIRE(result == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(view == nullptr);

    metal_release(parent);
}

TEST_CASE("metal_create_texture_view 上传到父纹理→从视图回读验证一致性", "[texture][view][upload][readback]")
{
    DeviceGuard dev_guard;

    // 创建父纹理
    constexpr uint32_t kTexWidth = 16;
    constexpr uint32_t kTexHeight = 16;

    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(parent != nullptr);

    // 创建全尺寸视图
    metal_texture* view = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1, 0, 1, &view);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view != nullptr);

    // 准备阶梯像素数据上传到父纹理
    constexpr uint32_t kBytesPerRow = kTexWidth * 4;
    constexpr uint32_t kDataSize = kTexHeight * kBytesPerRow;
    uint8_t src_data[kDataSize];
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            src_data[idx + 0] = static_cast<uint8_t>(x * 16);
            src_data[idx + 1] = static_cast<uint8_t>(y * 16);
            src_data[idx + 2] = 128;
            src_data[idx + 3] = 255;
        }
    }

    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, src_data, kDataSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        parent, upload_buf, 0, 0, 0,
        0, 0, 0, kTexWidth, kTexHeight, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 通过视图回读数据
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kDataSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        view, readback_buf, 0, 0, 0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 验证回读数据与原始数据一致
    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(mapped != nullptr);

    const uint8_t* readback_data = static_cast<const uint8_t*>(mapped);

    struct PixelCheck { uint32_t x, y; uint8_t r, g, b, a; };
    const PixelCheck checks[] = {
        { 0,  0,   0,   0, 128, 255},
        { 1,  1,  16,  16, 128, 255},
        { 7,  7, 112, 112, 128, 255},
        {15, 15, 240, 240, 128, 255},
        { 0, 15,   0, 240, 128, 255},
        {15,  0, 240,   0, 128, 255},
    };

    for (const auto& check : checks)
    {
        uint32_t idx = (check.y * kTexWidth + check.x) * 4;
        REQUIRE(readback_data[idx + 0] == check.r);
        REQUIRE(readback_data[idx + 1] == check.g);
        REQUIRE(readback_data[idx + 2] == check.b);
        REQUIRE(readback_data[idx + 3] == check.a);
    }

    metal_release(readback_buf);
    metal_release(upload_buf);
    metal_release(view);
    metal_release(parent);
}

TEST_CASE("metal_create_texture_view 上传到视图→从父纹理回读验证一致性", "[texture][view][upload][readback][reverse]")
{
    DeviceGuard dev_guard;

    constexpr uint32_t kTexWidth = 16;
    constexpr uint32_t kTexHeight = 16;

    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(parent != nullptr);

    // 创建全尺寸视图
    metal_texture* view = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1, 0, 1, &view);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view != nullptr);

    // 准备像素数据上传到视图
    constexpr uint32_t kBytesPerRow = kTexWidth * 4;
    constexpr uint32_t kDataSize = kTexHeight * kBytesPerRow;
    uint8_t src_data[kDataSize];
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            // 使用与父纹理测试不同的模式
            src_data[idx + 0] = static_cast<uint8_t>(255 - x * 16);
            src_data[idx + 1] = static_cast<uint8_t>(255 - y * 16);
            src_data[idx + 2] = 64;
            src_data[idx + 3] = 255;
        }
    }

    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, src_data, kDataSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    // 上传到视图（非父纹理）
    result = metal_texture_upload(
        view, upload_buf, 0, 0, 0,
        0, 0, 0, kTexWidth, kTexHeight, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 通过父纹理回读
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kDataSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        parent, readback_buf, 0, 0, 0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 验证父纹理包含视图写入的数据
    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* data = static_cast<const uint8_t*>(mapped);

    struct PixelCheck { uint32_t x, y; uint8_t r, g, b, a; };
    const PixelCheck checks[] = {
        { 0,  0, 255, 255,  64, 255},
        { 1,  1, 239, 239,  64, 255},
        { 7,  7, 143, 143,  64, 255},
        {15, 15,  15,  15,  64, 255},
        { 0, 15, 255,  15,  64, 255},
        {15,  0,  15, 255,  64, 255},
    };

    for (const auto& check : checks)
    {
        uint32_t idx = (check.y * kTexWidth + check.x) * 4;
        REQUIRE(data[idx + 0] == check.r);
        REQUIRE(data[idx + 1] == check.g);
        REQUIRE(data[idx + 2] == check.b);
        REQUIRE(data[idx + 3] == check.a);
    }

    metal_release(readback_buf);
    metal_release(upload_buf);
    metal_release(view);
    metal_release(parent);
}

TEST_CASE("metal_create_texture_view mip 子范围视图上传回读", "[texture][view][mipmap]")
{
    DeviceGuard dev_guard;

    // 创建有 3 级 mip 的父纹理 (16x16 → 8x8 → 4x4)
    constexpr uint32_t kBaseWidth = 16;
    constexpr uint32_t kBaseHeight = 16;
    constexpr uint32_t kTotalLevels = 3;

    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kBaseWidth, kBaseHeight, 1, kTotalLevels, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(parent != nullptr);

    // 创建只覆盖 level 1-2（8x8 和 4x4）的子范围视图
    metal_texture* view = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1,           // 全层
        1, 2,           // first_level=1, num_levels=2
        &view);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view != nullptr);

    // 验证视图有 2 级 mip，宽度为 8
    metal_texture_info view_info;
    result = metal_texture_get_info(view, &view_info);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view_info.width == 8);    // 从父纹理 level=1 开始
    REQUIRE(view_info.height == 8);
    REQUIRE(view_info.levels == 2);   // 2 级 (level 1-2)

    // 准备父纹理 level 1（8x8）的像素数据
    constexpr uint32_t kL1Width = 8;
    constexpr uint32_t kL1Height = 8;
    constexpr uint32_t kL1RowBytes = kL1Width * 4;
    constexpr uint32_t kL1Size = kL1Height * kL1RowBytes;
    uint8_t l1_data[kL1Size];
    for (uint32_t y = 0; y < kL1Height; ++y)
        for (uint32_t x = 0; x < kL1Width; ++x)
        {
            uint32_t idx = (y * kL1Width + x) * 4;
            l1_data[idx + 0] = static_cast<uint8_t>(x * 32);
            l1_data[idx + 1] = static_cast<uint8_t>(y * 32);
            l1_data[idx + 2] = 200;
            l1_data[idx + 3] = 255;
        }

    // 上传到父纹理的 level 1
    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, l1_data, kL1Size, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        parent, upload_buf, 0, 0, 1,  // parent level=1
        0, 0, 0, kL1Width, kL1Height, kL1RowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    // 通过视图回读 level 0（对应父纹理 level 1）
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kL1Size, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        view, readback_buf, 0, 0, 0, kL1RowBytes);  // view level=0
    REQUIRE(result == METAL_RESULT_OK);

    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* rb_data = static_cast<const uint8_t*>(mapped);
    // 验证 (0,0) 和 (4,4) 的像素值
    REQUIRE(rb_data[0] == 0);     // (0,0) R
    REQUIRE(rb_data[1] == 0);     // (0,0) G
    REQUIRE(rb_data[2] == 200);   // (0,0) B
    REQUIRE(rb_data[3] == 255);   // (0,0) A

    uint32_t idx44 = (4 * kL1Width + 4) * 4;
    REQUIRE(rb_data[idx44 + 0] == 128);  // (4,4) R = 4*32
    REQUIRE(rb_data[idx44 + 1] == 128);  // (4,4) G = 4*32
    REQUIRE(rb_data[idx44 + 2] == 200);  // (4,4) B
    REQUIRE(rb_data[idx44 + 3] == 255);  // (4,4) A

    metal_release(readback_buf);
    metal_release(upload_buf);
    metal_release(view);
    metal_release(parent);
}

TEST_CASE("metal_create_texture_view 视图的视图一致性", "[texture][view][nested]")
{
    DeviceGuard dev_guard;

    // 父纹理: 16x16, 2 mip levels
    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        16, 16, 1, 2, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(parent != nullptr);

    // 视图 1: level 1-1（从 8x8 开始）
    metal_texture* view1 = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1, 1, 1, &view1);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view1 != nullptr);

    metal_texture_info v1_info;
    metal_texture_get_info(view1, &v1_info);
    REQUIRE(v1_info.width == 8);
    REQUIRE(v1_info.levels == 1);

    // 视图 2: 从视图 1 的 level 0 再创建一个视图（视图的视图）
    // 注意：视图 1 只有 1 级 mip，所以只能从 0,1 开始
    metal_texture* view2 = nullptr;
    result = metal_create_texture_view(
        view1,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1, 0, 1, &view2);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view2 != nullptr);

    // 验证视图 2 的信息
    metal_texture_info v2_info;
    metal_texture_get_info(view2, &v2_info);
    REQUIRE(v2_info.width == 8);
    REQUIRE(v2_info.height == 8);
    REQUIRE(v2_info.levels == 1);
    REQUIRE(v2_info.pixel_format == METAL_PIXEL_FORMAT_RGBA8_UNORM);

    // 上传数据到视图 2，回读验证
    constexpr uint32_t kRowBytes = 8 * 4;
    constexpr uint32_t kDataSize = 8 * kRowBytes;
    uint8_t src_data[kDataSize];
    for (uint32_t i = 0; i < kDataSize; i += 4)
    {
        src_data[i + 0] = 100;
        src_data[i + 1] = 200;
        src_data[i + 2] = 50;
        src_data[i + 3] = 255;
    }

    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, src_data, kDataSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        view2, upload_buf, 0, 0, 0,
        0, 0, 0, 8, 8, kRowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    // 从父纹理回读 level 1，验证数据穿透到父纹理
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kDataSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        parent, readback_buf, 0, 0, 1, kRowBytes);  // parent level=1
    REQUIRE(result == METAL_RESULT_OK);

    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* data = static_cast<const uint8_t*>(mapped);
    REQUIRE(data[0] == 100);
    REQUIRE(data[1] == 200);
    REQUIRE(data[2] == 50);
    REQUIRE(data[3] == 255);

    metal_release(readback_buf);
    metal_release(upload_buf);
    metal_release(view2);
    metal_release(view1);
    metal_release(parent);
}

TEST_CASE("metal_create_texture_view 创建-释放循环无泄漏", "[texture][view][release]")
{
    DeviceGuard dev_guard;

    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        32, 32, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ,
        METAL_STORAGE_MODE_SHARED,
        &parent);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(parent != nullptr);

    for (int i = 0; i < 5; ++i)
    {
        metal_texture* view = nullptr;
        result = metal_create_texture_view(
            parent,
            METAL_PIXEL_FORMAT_RGBA8_UNORM,
            METAL_TEXTURE_TYPE_2D,
            0, 1, 0, 1, &view);

        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(view != nullptr);
        metal_release(view);
    }

    metal_release(parent);
}

TEST_CASE("metal_create_texture_view 区域上传到视图→从父纹理验证", "[texture][view][region]")
{
    DeviceGuard dev_guard;

    constexpr uint32_t kTexWidth = 16;
    constexpr uint32_t kTexHeight = 16;

    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(parent != nullptr);

    // 创建全尺寸视图
    metal_texture* view = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1, 0, 1, &view);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view != nullptr);

    // 先填充父纹理为全黑
    constexpr uint32_t kRowBytes = kTexWidth * 4;
    constexpr uint32_t kFullSize = kTexHeight * kRowBytes;
    uint8_t black_data[kFullSize];
    std::memset(black_data, 0, kFullSize);

    metal_buffer* black_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, black_data, kFullSize, METAL_STORAGE_MODE_SHARED, &black_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        parent, black_buf, 0, 0, 0,
        0, 0, 0, kTexWidth, kTexHeight, kRowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    // 准备一个子区域数据（8x8 红色块）
    constexpr uint32_t kRegionW = 8;
    constexpr uint32_t kRegionH = 8;
    constexpr uint32_t kRegionRowBytes = kRegionW * 4;
    constexpr uint32_t kRegionSize = kRegionH * kRegionRowBytes;
    uint8_t red_data[kRegionSize];
    for (uint32_t i = 0; i < kRegionSize; i += 4)
    {
        red_data[i + 0] = 255;   // R
        red_data[i + 1] = 0;     // G
        red_data[i + 2] = 0;     // B
        red_data[i + 3] = 255;   // A
    }

    // 通过视图在区域 (4,4) 处上传 8x8 红色块
    metal_buffer* region_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, red_data, kRegionSize, METAL_STORAGE_MODE_SHARED, &region_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        view, region_buf, 0, 0, 0,
        4, 4, 0,          // region_x=4, region_y=4
        kRegionW, kRegionH,
        kRegionRowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    // 从父纹理回读全图
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kFullSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        parent, readback_buf, 0, 0, 0, kRowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    // 验证
    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* data = static_cast<const uint8_t*>(mapped);

    // 黑色背景未受影响
    // (0,0) — 应为黑色
    REQUIRE(data[0] == 0);
    REQUIRE(data[1] == 0);
    REQUIRE(data[2] == 0);

    // (7,0) — 黑色（红色区域从 x=4 开始，y=4 开始）
    uint32_t idx_7_0 = (0 * kTexWidth + 7) * 4;
    REQUIRE(data[idx_7_0 + 0] == 0);

    // (4,4) — 红色区域左上角
    uint32_t idx_4_4 = (4 * kTexWidth + 4) * 4;
    REQUIRE(data[idx_4_4 + 0] == 255);
    REQUIRE(data[idx_4_4 + 1] == 0);
    REQUIRE(data[idx_4_4 + 2] == 0);

    // (11,11) — 红色区域右下角
    uint32_t idx_11_11 = (11 * kTexWidth + 11) * 4;
    REQUIRE(data[idx_11_11 + 0] == 255);
    REQUIRE(data[idx_11_11 + 1] == 0);
    REQUIRE(data[idx_11_11 + 2] == 0);

    // (15,15) — 黑色（超出红色区域）
    uint32_t idx_15_15 = (15 * kTexWidth + 15) * 4;
    REQUIRE(data[idx_15_15 + 0] == 0);

    // 红色区域边界验证
    // (3,4) — 红色区域左边一个像素，应为黑色
    uint32_t idx_3_4 = (4 * kTexWidth + 3) * 4;
    REQUIRE(data[idx_3_4 + 0] == 0);
    REQUIRE(data[idx_3_4 + 1] == 0);
    REQUIRE(data[idx_3_4 + 2] == 0);

    // (4,3) — 红色区域上方一个像素，应为黑色
    uint32_t idx_4_3 = (3 * kTexWidth + 4) * 4;
    REQUIRE(data[idx_4_3 + 0] == 0);
    REQUIRE(data[idx_4_3 + 1] == 0);
    REQUIRE(data[idx_4_3 + 2] == 0);

    metal_release(readback_buf);
    metal_release(region_buf);
    metal_release(black_buf);
    metal_release(view);
    metal_release(parent);
}

TEST_CASE("metal_create_texture_view 父纹理释放后视图仍有效", "[texture][view][lifetime]")
{
    DeviceGuard dev_guard;

    constexpr uint32_t kTexWidth = 8;
    constexpr uint32_t kTexHeight = 8;
    constexpr uint32_t kRowBytes = kTexWidth * 4;
    constexpr uint32_t kDataSize = kTexHeight * kRowBytes;

    // 创建父纹理
    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, 1, 1, 1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(parent != nullptr);

    // 创建视图
    metal_texture* view = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,
        0, 1, 0, 1, &view);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view != nullptr);

    // 上传数据到父纹理
    uint8_t src_data[kDataSize];
    for (uint32_t i = 0; i < kDataSize; i += 4)
    {
        src_data[i + 0] = 50;
        src_data[i + 1] = 100;
        src_data[i + 2] = 150;
        src_data[i + 3] = 255;
    }

    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, src_data, kDataSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        parent, upload_buf, 0, 0, 0,
        0, 0, 0, kTexWidth, kTexHeight, kRowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    // 释放父纹理
    metal_release(parent);
    parent = nullptr;

    // 通过视图仍然可以回读数据（视图持有自己的 retain）
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kDataSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        view, readback_buf, 0, 0, 0, kRowBytes);
    REQUIRE(result == METAL_RESULT_OK);

    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* data = static_cast<const uint8_t*>(mapped);
    // 验证视图仍然可以正确读取数据
    REQUIRE(data[0] == 50);
    REQUIRE(data[1] == 100);
    REQUIRE(data[2] == 150);
    REQUIRE(data[3] == 255);

    metal_release(readback_buf);
    metal_release(upload_buf);
    metal_release(view);
}

TEST_CASE("metal_create_texture_view 2D 数组子层视图上传回读", "[texture][view][array][layer]")
{
    DeviceGuard dev_guard;

    // 创建 2D 数组纹理：16x16，4 层
    constexpr uint32_t kTexWidth = 16;
    constexpr uint32_t kTexHeight = 16;
    constexpr uint32_t kNumLayers = 4;
    constexpr uint32_t kTargetLayer = 2;  // 视图覆盖的层

    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, kNumLayers, 1, 1,
        METAL_TEXTURE_TYPE_2D_ARRAY,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);

    // 某些硬件可能不支持 array 纹理
    if (result != METAL_RESULT_OK)
    {
        REQUIRE(result == METAL_RESULT_UNSUPPORTED);
        return;
    }
    REQUIRE(parent != nullptr);

    // 创建单层视图（只覆盖 layer 2）
    metal_texture* view = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,   // 视图为 2D（非 Array）
        kTargetLayer, 1,          // first_layer=2, num_layers=1
        0, 1,                     // first_level=0, num_levels=1
        &view);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view != nullptr);
    REQUIRE(view != parent);

    // 验证视图信息：深度为 1（单层），类型为 2D（非 Array）
    metal_texture_info view_info;
    result = metal_texture_get_info(view, &view_info);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view_info.width == kTexWidth);
    REQUIRE(view_info.height == kTexHeight);
    REQUIRE(view_info.depth == 1);   // 单层视图
    REQUIRE(view_info.levels == 1);
    REQUIRE(view_info.type == METAL_TEXTURE_TYPE_2D);  // Metal 将单层视图降为 2D 类型

    // 阶段 1：上传数据到父纹理的 layer 2，从视图回读验证
    constexpr uint32_t kBytesPerRow = kTexWidth * 4;
    constexpr uint32_t kLayerSize = kTexHeight * kBytesPerRow;
    uint8_t parent_data[kLayerSize];

    // 层 2 数据的特征模式：R=200, G=100, B=50+(x+y), A=255
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            parent_data[idx + 0] = 200;
            parent_data[idx + 1] = 100;
            parent_data[idx + 2] = static_cast<uint8_t>(50 + x + y);
            parent_data[idx + 3] = 255;
        }
    }

    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, parent_data, kLayerSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    // 上传到父纹理的 layer 2
    result = metal_texture_upload(
        parent, upload_buf, 0,
        kTargetLayer,  // layer=2（父纹理的层索引）
        0,             // level=0
        0, 0, 0,
        kTexWidth, kTexHeight,
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 通过视图回读（视图的 layer=0 对应父纹理的 layer=2）
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kLayerSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        view, readback_buf, 0,
        0,  // view layer=0
        0,  // level=0
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 验证视图回读数据与上传到父纹理的数据一致
    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* view_readback = static_cast<const uint8_t*>(mapped);
    REQUIRE(view_readback[0] == 200);                      // (0,0) R
    REQUIRE(view_readback[1] == 100);                      // (0,0) G
    REQUIRE(view_readback[2] == 50);                       // (0,0) B = 50+0+0
    REQUIRE(view_readback[3] == 255);                      // (0,0) A

    uint32_t idx_5_5 = (5 * kTexWidth + 5) * 4;
    REQUIRE(view_readback[idx_5_5 + 0] == 200);            // (5,5) R
    REQUIRE(view_readback[idx_5_5 + 1] == 100);            // (5,5) G
    REQUIRE(view_readback[idx_5_5 + 2] == 60);             // (5,5) B = 50+5+5
    REQUIRE(view_readback[idx_5_5 + 3] == 255);            // (5,5) A

    // 验证所有像素的 R=200, G=100, A=255
    for (uint32_t y = 0; y < kTexHeight; ++y)
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            REQUIRE(view_readback[idx + 0] == 200);
            REQUIRE(view_readback[idx + 1] == 100);
            REQUIRE(view_readback[idx + 3] == 255);
        }

    metal_unmap_buffer(readback_buf);
    mapped = nullptr;
    metal_release(readback_buf);
    metal_release(upload_buf);

    // 阶段 2：上传数据到视图，从父纹理 layer 2 回读验证
    uint8_t view_upload_data[kLayerSize];
    // 视图上传数据：R=50, G=150, B=200+y, A=255
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            view_upload_data[idx + 0] = 50;
            view_upload_data[idx + 1] = 150;
            view_upload_data[idx + 2] = static_cast<uint8_t>(200 + y);
            view_upload_data[idx + 3] = 255;
        }
    }

    metal_buffer* view_upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, view_upload_data, kLayerSize, METAL_STORAGE_MODE_SHARED, &view_upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    // 上传到视图（视图的 layer=0）
    result = metal_texture_upload(
        view, view_upload_buf, 0,
        0,  // view layer=0
        0,  // level=0
        0, 0, 0,
        kTexWidth, kTexHeight,
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 从父纹理 layer 2 回读，验证数据从视图穿透
    metal_buffer* parent_readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kLayerSize, METAL_STORAGE_MODE_SHARED, &parent_readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        parent, parent_readback_buf, 0,
        kTargetLayer,  // parent layer=2
        0,             // level=0
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_map_buffer(parent_readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* parent_layer2 = static_cast<const uint8_t*>(mapped);
    REQUIRE(parent_layer2[0] == 50);                       // (0,0) R
    REQUIRE(parent_layer2[1] == 150);                      // (0,0) G
    REQUIRE(parent_layer2[2] == 200);                      // (0,0) B = 200+0
    REQUIRE(parent_layer2[3] == 255);                      // (0,0) A

    uint32_t idx_3_7 = (7 * kTexWidth + 3) * 4;
    REQUIRE(parent_layer2[idx_3_7 + 0] == 50);             // (3,7) R
    REQUIRE(parent_layer2[idx_3_7 + 1] == 150);            // (3,7) G
    REQUIRE(parent_layer2[idx_3_7 + 2] == 207);            // (3,7) B = 200+7

    metal_unmap_buffer(parent_readback_buf);
    metal_release(parent_readback_buf);

    // 阶段 3：验证父纹理其他层（0,1,3）没有被视图写入影响
    // 父纹理的层 2 之前被视图写入了 R=50 数据，而其他层未写入
    // 所以回读 layer 0 应该得到原始初始化值（全零 Metal 默认值）
    // 注意：Metal Shared 模式纹理初始内容未定义，所以我们只验证
    // layer 0/1/3 的内容与 layer 2 不同（确认层隔离）

    metal_buffer* other_layer_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kLayerSize, METAL_STORAGE_MODE_SHARED, &other_layer_buf);
    REQUIRE(result == METAL_RESULT_OK);

    // 验证 layer 0 不是我们写入视图的特征值 50/150
    result = metal_texture_readback(
        parent, other_layer_buf, 0,
        0,  // layer=0
        0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_map_buffer(other_layer_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* layer0 = static_cast<const uint8_t*>(mapped);
    // layer 0 的第一个像素 R 不应该等于 50（那是 layer 2 的数据）
    REQUIRE(layer0[0] != 50);
    metal_unmap_buffer(other_layer_buf);

    // 验证 layer 1 也不是 50
    result = metal_texture_readback(
        parent, other_layer_buf, 0,
        1,  // layer=1
        0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);
    result = metal_map_buffer(other_layer_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* layer1 = static_cast<const uint8_t*>(mapped);
    REQUIRE(layer1[0] != 50);
    metal_unmap_buffer(other_layer_buf);

    // 验证 layer 3 也不是 50
    result = metal_texture_readback(
        parent, other_layer_buf, 0,
        3,  // layer=3
        0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);
    result = metal_map_buffer(other_layer_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* layer3 = static_cast<const uint8_t*>(mapped);
    REQUIRE(layer3[0] != 50);
    metal_unmap_buffer(other_layer_buf);

    metal_release(other_layer_buf);
    metal_release(view_upload_buf);
    metal_release(view);
    metal_release(parent);
}

TEST_CASE("metal_create_texture_view Cubemap 单 face 子视图上传回读", "[texture][view][cube][face]")
{
    DeviceGuard dev_guard;

    // 创建 Cubemap 纹理：16x16，6 个 face
    constexpr uint32_t kTexWidth = 16;
    constexpr uint32_t kTexHeight = 16;
    constexpr uint32_t kTargetFace = 3;  // 视图覆盖的 face（-Y 方向）

    metal_texture* parent = nullptr;
    metal_result result = metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        kTexWidth, kTexHeight, 1, 1, 1,
        METAL_TEXTURE_TYPE_CUBE,
        METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &parent);

    // 某些硬件可能不支持 cube 纹理
    if (result != METAL_RESULT_OK)
    {
        REQUIRE(result == METAL_RESULT_UNSUPPORTED);
        return;
    }
    REQUIRE(parent != nullptr);

    // 创建单 face 视图（只覆盖 face 3）
    metal_texture* view = nullptr;
    result = metal_create_texture_view(
        parent,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_TEXTURE_TYPE_2D,   // 视图为 2D（非 Cube）
        kTargetFace, 1,           // first_layer=3, num_layers=1
        0, 1,                     // first_level=0, num_levels=1
        &view);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view != nullptr);
    REQUIRE(view != parent);

    // 验证视图信息：16x16, depth=1, type=2D
    metal_texture_info view_info;
    result = metal_texture_get_info(view, &view_info);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(view_info.width == kTexWidth);
    REQUIRE(view_info.height == kTexHeight);
    REQUIRE(view_info.depth == 1);
    REQUIRE(view_info.levels == 1);
    REQUIRE(view_info.type == METAL_TEXTURE_TYPE_2D);
    REQUIRE(view_info.pixel_format == METAL_PIXEL_FORMAT_RGBA8_UNORM);
    REQUIRE(view_info.storage_mode == METAL_STORAGE_MODE_SHARED);

    // 阶段 1：上传数据到父纹理的 face 3，从视图回读验证
    constexpr uint32_t kBytesPerRow = kTexWidth * 4;
    constexpr uint32_t kFaceSize = kTexHeight * kBytesPerRow;
    uint8_t face_data[kFaceSize];

    // face 3 数据的特征模式：R=180, G=90, B=120+x, A=255
    for (uint32_t y = 0; y < kTexHeight; ++y)
    {
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            face_data[idx + 0] = 180;
            face_data[idx + 1] = 90;
            face_data[idx + 2] = static_cast<uint8_t>(120 + x);
            face_data[idx + 3] = 255;
        }
    }

    metal_buffer* upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, face_data, kFaceSize, METAL_STORAGE_MODE_SHARED, &upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    // 上传到父纹理的 face 3（Cube face 用 layer 参数选择）
    result = metal_texture_upload(
        parent, upload_buf, 0,
        kTargetFace,  // layer=3（face 索引）
        0,            // level=0
        0, 0, 0,
        kTexWidth, kTexHeight,
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 通过视图回读（视图的 layer=0 对应父纹理的 face 3）
    metal_buffer* readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kFaceSize, METAL_STORAGE_MODE_SHARED, &readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        view, readback_buf, 0,
        0,  // view layer=0
        0,  // level=0
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    void* mapped = nullptr;
    result = metal_map_buffer(readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* view_readback = static_cast<const uint8_t*>(mapped);
    REQUIRE(view_readback[0] == 180);   // (0,0) R
    REQUIRE(view_readback[1] == 90);    // (0,0) G
    REQUIRE(view_readback[2] == 120);   // (0,0) B = 120+0
    REQUIRE(view_readback[3] == 255);   // (0,0) A

    // 验证所有像素 R=180, G=90, A=255, B=120+x
    for (uint32_t y = 0; y < kTexHeight; ++y)
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            REQUIRE(view_readback[idx + 0] == 180);
            REQUIRE(view_readback[idx + 1] == 90);
            REQUIRE(view_readback[idx + 2] == static_cast<uint8_t>(120 + x));
            REQUIRE(view_readback[idx + 3] == 255);
        }

    metal_unmap_buffer(readback_buf);
    metal_release(readback_buf);
    metal_release(upload_buf);

    // 阶段 2：上传数据到视图，从父纹理 face 3 回读验证
    uint8_t view_upload_data[kFaceSize];
    // 视图上传数据：R=30, G=210, B=240+y, A=255
    for (uint32_t y = 0; y < kTexHeight; ++y)
        for (uint32_t x = 0; x < kTexWidth; ++x)
        {
            uint32_t idx = (y * kTexWidth + x) * 4;
            view_upload_data[idx + 0] = 30;
            view_upload_data[idx + 1] = 210;
            view_upload_data[idx + 2] = static_cast<uint8_t>(240 + y);
            view_upload_data[idx + 3] = 255;
        }

    metal_buffer* view_upload_buf = nullptr;
    result = metal_create_buffer_with_bytes(
        dev_guard.dev, view_upload_data, kFaceSize,
        METAL_STORAGE_MODE_SHARED, &view_upload_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_upload(
        view, view_upload_buf, 0,
        0,  // view layer=0
        0,  // level=0
        0, 0, 0,
        kTexWidth, kTexHeight,
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    // 从父纹理 face 3 回读验证数据穿透
    metal_buffer* parent_readback_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kFaceSize, METAL_STORAGE_MODE_SHARED, &parent_readback_buf);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_texture_readback(
        parent, parent_readback_buf, 0,
        kTargetFace,  // parent face=3
        0,            // level=0
        kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_map_buffer(parent_readback_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);

    const uint8_t* parent_face3 = static_cast<const uint8_t*>(mapped);
    REQUIRE(parent_face3[0] == 30);     // (0,0) R
    REQUIRE(parent_face3[1] == 210);    // (0,0) G
    REQUIRE(parent_face3[2] == 240);    // (0,0) B = 240+0
    REQUIRE(parent_face3[3] == 255);    // (0,0) A

    uint32_t idx_5_3 = (3 * kTexWidth + 5) * 4;
    REQUIRE(parent_face3[idx_5_3 + 0] == 30);     // (5,3) R
    REQUIRE(parent_face3[idx_5_3 + 1] == 210);    // (5,3) G
    REQUIRE(parent_face3[idx_5_3 + 2] == 243);    // (5,3) B = 240+3
    REQUIRE(parent_face3[idx_5_3 + 3] == 255);    // (5,3) A

    metal_unmap_buffer(parent_readback_buf);
    metal_release(parent_readback_buf);

    // 阶段 3：验证其他 face（0,1,2,4,5）没有被视图写入影响
    metal_buffer* other_face_buf = nullptr;
    result = metal_create_buffer(
        dev_guard.dev, kFaceSize, METAL_STORAGE_MODE_SHARED, &other_face_buf);
    REQUIRE(result == METAL_RESULT_OK);

    // 检查 face 0 的第一个像素 R 不等于 30（那是 face 3 的数据）
    result = metal_texture_readback(
        parent, other_face_buf, 0,
        0,  // face=0
        0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);

    result = metal_map_buffer(other_face_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* face0 = static_cast<const uint8_t*>(mapped);
    REQUIRE(face0[0] != 30);
    metal_unmap_buffer(other_face_buf);

    // 检查 face 1
    result = metal_texture_readback(
        parent, other_face_buf, 0,
        1,  // face=1
        0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);
    result = metal_map_buffer(other_face_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* face1 = static_cast<const uint8_t*>(mapped);
    REQUIRE(face1[0] != 30);
    metal_unmap_buffer(other_face_buf);

    // 检查 face 2
    result = metal_texture_readback(
        parent, other_face_buf, 0,
        2,  // face=2
        0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);
    result = metal_map_buffer(other_face_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* face2 = static_cast<const uint8_t*>(mapped);
    REQUIRE(face2[0] != 30);
    metal_unmap_buffer(other_face_buf);

    // 检查 face 4
    result = metal_texture_readback(
        parent, other_face_buf, 0,
        4,  // face=4
        0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);
    result = metal_map_buffer(other_face_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* face4 = static_cast<const uint8_t*>(mapped);
    REQUIRE(face4[0] != 30);
    metal_unmap_buffer(other_face_buf);

    // 检查 face 5
    result = metal_texture_readback(
        parent, other_face_buf, 0,
        5,  // face=5
        0, kBytesPerRow);
    REQUIRE(result == METAL_RESULT_OK);
    result = metal_map_buffer(other_face_buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    const uint8_t* face5 = static_cast<const uint8_t*>(mapped);
    REQUIRE(face5[0] != 30);
    metal_unmap_buffer(other_face_buf);

    metal_release(other_face_buf);
    metal_release(view_upload_buf);
    metal_release(view);
    metal_release(parent);
}
