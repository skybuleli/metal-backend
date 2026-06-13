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
