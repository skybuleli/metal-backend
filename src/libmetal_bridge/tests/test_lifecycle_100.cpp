// test_lifecycle_100.cpp — P4.5.3 资源生命周期验收
//
// Buffer/Texture 创建→使用→销毁 100 次无泄漏。
// 每次循环：创建 → 操作（map/get_info） → 释放。
//
// 环境：需真实 GPU，Catch2 独立测试二进制。

#include <catch2/catch_test_macros.hpp>

#include "metal_bridge.h"
#include "metal_limits.h"

#include <cstring>
#include <cstdint>

// ── RAII 辅助 ──

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

// ── 100 次 Buffer 创建→使用→释放 ──

TEST_CASE("Buffer 创建→map→释放 100 次无泄漏", "[buffer][lifecycle][100]")
{
    DeviceGuard dev_guard;

    for (int i = 0; i < 100; ++i)
    {
        metal_buffer* buf = nullptr;
        REQUIRE(metal_create_buffer(dev_guard.dev, 256, METAL_STORAGE_MODE_SHARED, &buf) == METAL_RESULT_OK);
        REQUIRE(buf != nullptr);

        // 使用：map → 写入 1 字节 → unmap
        void* ptr = nullptr;
        REQUIRE(metal_map_buffer(buf, &ptr) == METAL_RESULT_OK);
        REQUIRE(ptr != nullptr);
        static_cast<uint8_t*>(ptr)[0] = static_cast<uint8_t>(i);
        metal_unmap_buffer(buf);

        metal_release(buf);
    }
}

TEST_CASE("Buffer 创建→get_info→释放 100 次无泄漏", "[buffer][lifecycle][100]")
{
    DeviceGuard dev_guard;

    for (int i = 0; i < 100; ++i)
    {
        metal_buffer* buf = nullptr;
        REQUIRE(metal_create_buffer(dev_guard.dev, 4096, METAL_STORAGE_MODE_SHARED, &buf) == METAL_RESULT_OK);
        REQUIRE(buf != nullptr);

        metal_buffer_info info;
        std::memset(&info, 0xFF, sizeof(info));
        REQUIRE(metal_buffer_get_info(buf, &info) == METAL_RESULT_OK);
        REQUIRE(info.size >= 4096);
        REQUIRE(info.storage_mode == METAL_STORAGE_MODE_SHARED);

        metal_release(buf);
    }
}

// ── 100 次 Texture 创建→使用→释放 ──

TEST_CASE("Texture 创建→get_info→释放 100 次无泄漏", "[texture][lifecycle][100]")
{
    DeviceGuard dev_guard;

    for (int i = 0; i < 100; ++i)
    {
        metal_texture* tex = nullptr;
        metal_result result = metal_create_texture(
            dev_guard.dev,
            METAL_PIXEL_FORMAT_RGBA8_UNORM,
            64, 64, 1, 1, 1,
            METAL_TEXTURE_TYPE_2D,
            METAL_TEXTURE_USAGE_SHADER_READ | METAL_TEXTURE_USAGE_RENDER_TARGET,
            METAL_STORAGE_MODE_SHARED,
            &tex);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(tex != nullptr);

        metal_texture_info info;
        std::memset(&info, 0xFF, sizeof(info));
        REQUIRE(metal_texture_get_info(tex, &info) == METAL_RESULT_OK);
        REQUIRE(info.width == 64);
        REQUIRE(info.height == 64);
        REQUIRE(info.pixel_format == METAL_PIXEL_FORMAT_RGBA8_UNORM);

        metal_release(tex);
    }
}

TEST_CASE("Texture 多种格式创建 100 次无泄漏", "[texture][lifecycle][100]")
{
    DeviceGuard dev_guard;

    metal_pixel_format formats[] = {
        METAL_PIXEL_FORMAT_BGRA8_UNORM,
        METAL_PIXEL_FORMAT_RGBA8_UNORM,
        METAL_PIXEL_FORMAT_R8_UNORM,
        METAL_PIXEL_FORMAT_RG8_UNORM,
        METAL_PIXEL_FORMAT_BGRA8_SRGB,
        METAL_PIXEL_FORMAT_D32_FLOAT,
        METAL_PIXEL_FORMAT_BC1_RGBA,
    };
    constexpr int kNumFormats = sizeof(formats) / sizeof(formats[0]);

    for (int i = 0; i < 100; ++i)
    {
        metal_pixel_format fmt = formats[i % kNumFormats];
        metal_texture* tex = nullptr;
        metal_result result = metal_create_texture(
            dev_guard.dev,
            fmt,
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

// ── 100 次 Buffer+Texture 混合创建→释放 ──

TEST_CASE("Buffer+Texture 混合创建释放 100 次无泄漏", "[combined][lifecycle][100]")
{
    DeviceGuard dev_guard;

    for (int i = 0; i < 100; ++i)
    {
        // Buffer
        metal_buffer* buf = nullptr;
        REQUIRE(metal_create_buffer(dev_guard.dev, 512, METAL_STORAGE_MODE_SHARED, &buf) == METAL_RESULT_OK);
        REQUIRE(buf != nullptr);

        // Texture
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

        // 分别释放
        metal_release(buf);
        metal_release(tex);
    }
}
