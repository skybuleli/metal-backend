// test_presenter.cpp — P4.4.3 CAMetalLayer Presenter 回归测试
//
// 测试目标：
//   1. metal_create_presenter / metal_presenter_get_info / metal_presenter_resize 可用
//   2. 空参数返回 INVALID_ARGUMENT
//   3. 不支持的纹理格式在进入 drawable 提交前返回 UNSUPPORTED

#include <catch2/catch_test_macros.hpp>

#include <QuartzCore/QuartzCore.hpp>

#include "metal_bridge.h"

struct DeviceGuard
{
    metal_device* dev = nullptr;

    DeviceGuard()
    {
        REQUIRE(metal_create_device(&dev) == METAL_RESULT_OK);
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

TEST_CASE("Presenter 可创建并维护 drawable 状态", "[presenter][create][resize]")
{
    DeviceGuard dev_guard;

    CA::MetalLayer* layer = CA::MetalLayer::layer();
    REQUIRE(layer != nullptr);

    metal_presenter* presenter = nullptr;
    REQUIRE(metal_create_presenter(dev_guard.dev, layer, &presenter) == METAL_RESULT_OK);
    REQUIRE(presenter != nullptr);

    metal_presenter_info info;
    REQUIRE(metal_presenter_get_info(presenter, &info) == METAL_RESULT_OK);
    REQUIRE(info.abi_version == METAL_BRIDGE_ABI_VERSION);
    REQUIRE(info.drawable_width == 0);
    REQUIRE(info.drawable_height == 0);
    REQUIRE(info.pixel_format == METAL_PIXEL_FORMAT_BGRA8_UNORM);

    REQUIRE(metal_presenter_resize(presenter, 320, 180) == METAL_RESULT_OK);
    REQUIRE(metal_presenter_get_info(presenter, &info) == METAL_RESULT_OK);
    REQUIRE(info.drawable_width == 320);
    REQUIRE(info.drawable_height == 180);
    REQUIRE(info.pixel_format == METAL_PIXEL_FORMAT_BGRA8_UNORM);

    metal_release(presenter);
}

TEST_CASE("Presenter 空参数返回 INVALID_ARGUMENT", "[presenter][error]")
{
    metal_presenter* presenter = nullptr;
    metal_presenter_info info;

    REQUIRE(metal_create_presenter(nullptr, nullptr, &presenter) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_create_presenter(nullptr, reinterpret_cast<void*>(0x1), nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_presenter_get_info(nullptr, &info) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_presenter_get_info(presenter, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_presenter_resize(nullptr, 1, 1) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_presenter_resize(presenter, 0, 1) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_presenter_resize(presenter, 1, 0) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_presenter_present_texture(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("Presenter 对不支持的纹理格式返回 UNSUPPORTED", "[presenter][format]")
{
    DeviceGuard dev_guard;

    CA::MetalLayer* layer = CA::MetalLayer::layer();
    REQUIRE(layer != nullptr);

    metal_presenter* presenter = nullptr;
    REQUIRE(metal_create_presenter(dev_guard.dev, layer, &presenter) == METAL_RESULT_OK);
    REQUIRE(metal_presenter_resize(presenter, 16, 16) == METAL_RESULT_OK);

    metal_texture* texture = nullptr;
    REQUIRE(metal_create_texture(
        dev_guard.dev,
        METAL_PIXEL_FORMAT_R8_UNORM,
        16,
        16,
        1,
        1,
        1,
        METAL_TEXTURE_TYPE_2D,
        METAL_TEXTURE_USAGE_RENDER_TARGET,
        METAL_STORAGE_MODE_SHARED,
        &texture) == METAL_RESULT_OK);
    REQUIRE(texture != nullptr);

    REQUIRE(metal_presenter_present_texture(presenter, texture) == METAL_RESULT_UNSUPPORTED);

    metal_release(texture);
    metal_release(presenter);
}
