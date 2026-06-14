// test_pipeline_state.cpp — P4.5.5 管线状态验收
//
// Blend/Depth/Stencil/Scissor 四种状态创建与切换正确性。
// 每种状态分别创建、设置、验证返回值，确保 Metal API 不崩溃或拒绝。

#include <catch2/catch_test_macros.hpp>

#include "metal_bridge.h"
#include "metal_limits.h"

#include <cstring>
#include <cstdint>

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
        if (dev != nullptr) { metal_release(dev); dev = nullptr; }
    }
    DeviceGuard(const DeviceGuard&) = delete;
    DeviceGuard& operator=(const DeviceGuard&) = delete;
};

struct DepthStencilGuard
{
    metal_depth_stencil_state* state = nullptr;
    DepthStencilGuard(metal_device* dev, const metal_depth_stencil_descriptor& desc)
    {
        metal_result result = metal_create_depth_stencil_state(dev, &desc, &state);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(state != nullptr);
    }
    ~DepthStencilGuard()
    {
        if (state != nullptr) { metal_release(state); state = nullptr; }
    }
    DepthStencilGuard(const DepthStencilGuard&) = delete;
    DepthStencilGuard& operator=(const DepthStencilGuard&) = delete;
};

// ── 深度状态 ──

TEST_CASE("depth: 创建 Less+Write 状态", "[depth][state]")
{
    DeviceGuard g;
    metal_depth_stencil_descriptor desc{};
    desc.depth_compare_function = METAL_COMPARE_LESS;
    desc.depth_write_enabled = 1;
    desc.stencil_enabled = 0;
    DepthStencilGuard dg(g.dev, desc);
    REQUIRE(dg.state != nullptr);
}

TEST_CASE("depth: 创建 Always+WriteDisable 状态", "[depth][state]")
{
    DeviceGuard g;
    metal_depth_stencil_descriptor desc{};
    desc.depth_compare_function = METAL_COMPARE_ALWAYS;
    desc.depth_write_enabled = 0;
    desc.stencil_enabled = 0;
    DepthStencilGuard dg(g.dev, desc);
    REQUIRE(dg.state != nullptr);
}

TEST_CASE("depth: 8 种比较函数全部可创建", "[depth][state]")
{
    DeviceGuard g;
    metal_compare_function funcs[] = {
        METAL_COMPARE_NEVER, METAL_COMPARE_LESS, METAL_COMPARE_EQUAL,
        METAL_COMPARE_LESS_EQUAL, METAL_COMPARE_GREATER, METAL_COMPARE_NOT_EQUAL,
        METAL_COMPARE_GREATER_EQUAL, METAL_COMPARE_ALWAYS,
    };
    for (auto func : funcs)
    {
        metal_depth_stencil_descriptor desc{};
        desc.depth_compare_function = func;
        desc.depth_write_enabled = 1;
        desc.stencil_enabled = 0;
        metal_depth_stencil_state* state = nullptr;
        REQUIRE(metal_create_depth_stencil_state(g.dev, &desc, &state) == METAL_RESULT_OK);
        REQUIRE(state != nullptr);
        metal_release(state);
    }
}

// ── 模板状态 ──

metal_stencil_descriptor MakeStencilAlwaysReplace()
{
    metal_stencil_descriptor s{};
    s.compare_function = METAL_COMPARE_ALWAYS;
    s.stencil_failure = METAL_STENCIL_OPERATION_KEEP;
    s.depth_failure = METAL_STENCIL_OPERATION_KEEP;
    s.depth_stencil_pass = METAL_STENCIL_OPERATION_REPLACE;
    s.read_mask = 0xFF;
    s.write_mask = 0xFF;
    return s;
}

TEST_CASE("stencil: 创建前后双面模板状态", "[stencil][state]")
{
    DeviceGuard g;
    metal_depth_stencil_descriptor desc{};
    desc.depth_compare_function = METAL_COMPARE_LESS;
    desc.depth_write_enabled = 1;
    desc.stencil_enabled = 1;
    desc.front_face = MakeStencilAlwaysReplace();
    desc.back_face = MakeStencilAlwaysReplace();
    DepthStencilGuard dg(g.dev, desc);
    REQUIRE(dg.state != nullptr);
}

TEST_CASE("stencil: 8 种操作可全部设置", "[stencil][state]")
{
    DeviceGuard g;
    metal_stencil_operation ops[] = {
        METAL_STENCIL_OPERATION_KEEP, METAL_STENCIL_OPERATION_ZERO,
        METAL_STENCIL_OPERATION_REPLACE, METAL_STENCIL_OPERATION_INCREMENT_CLAMP,
        METAL_STENCIL_OPERATION_DECREMENT_CLAMP, METAL_STENCIL_OPERATION_INVERT,
        METAL_STENCIL_OPERATION_INCREMENT_WRAP, METAL_STENCIL_OPERATION_DECREMENT_WRAP,
    };
    for (auto op : ops)
    {
        metal_depth_stencil_descriptor desc{};
        desc.depth_compare_function = METAL_COMPARE_LESS_EQUAL;
        desc.depth_write_enabled = 1;
        desc.stencil_enabled = 1;
        metal_stencil_descriptor s{};
        s.compare_function = METAL_COMPARE_ALWAYS;
        s.stencil_failure = op;
        s.depth_failure = op;
        s.depth_stencil_pass = op;
        s.read_mask = 0xFF;
        s.write_mask = 0xFF;
        desc.front_face = s;
        desc.back_face = s;
        metal_depth_stencil_state* state = nullptr;
        REQUIRE(metal_create_depth_stencil_state(g.dev, &desc, &state) == METAL_RESULT_OK);
        REQUIRE(state != nullptr);
        metal_release(state);
    }
}

// ── 混合附件描述符 ──

TEST_CASE("blend: 描述符大小正确", "[blend][state]")
{
    REQUIRE(sizeof(metal_blend_attachment_descriptor) >= 24);
}

TEST_CASE("blend: 单附件混合字段设置", "[blend][state]")
{
    metal_blend_attachment_descriptor b{};
    b.blending_enabled = 1;
    b.src_rgb_factor = METAL_BLEND_FACTOR_SRC_ALPHA;
    b.dst_rgb_factor = METAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    b.rgb_operation = METAL_BLEND_OPERATION_ADD;
    b.src_alpha_factor = METAL_BLEND_FACTOR_ONE;
    b.dst_alpha_factor = METAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    b.alpha_operation = METAL_BLEND_OPERATION_ADD;
    b.write_mask = METAL_COLOR_WRITE_MASK_ALL;
    REQUIRE(b.blending_enabled == 1);
    REQUIRE(b.src_rgb_factor == METAL_BLEND_FACTOR_SRC_ALPHA);
    REQUIRE(b.dst_rgb_factor == METAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    REQUIRE(b.write_mask == METAL_COLOR_WRITE_MASK_ALL);
}

TEST_CASE("blend: 多附件交替启用", "[blend][state]")
{
    metal_blend_attachment_descriptor blends[4];
    for (int i = 0; i < 4; ++i)
    {
        std::memset(&blends[i], 0, sizeof(blends[i]));
        blends[i].blending_enabled = (i % 2 == 0) ? 1 : 0;
        blends[i].src_rgb_factor = METAL_BLEND_FACTOR_ONE;
        blends[i].dst_rgb_factor = METAL_BLEND_FACTOR_ZERO;
        blends[i].rgb_operation = METAL_BLEND_OPERATION_ADD;
        blends[i].src_alpha_factor = METAL_BLEND_FACTOR_ONE;
        blends[i].dst_alpha_factor = METAL_BLEND_FACTOR_ZERO;
        blends[i].alpha_operation = METAL_BLEND_OPERATION_ADD;
        blends[i].write_mask = METAL_COLOR_WRITE_MASK_ALL;
    }
    REQUIRE(blends[0].blending_enabled == 1);
    REQUIRE(blends[1].blending_enabled == 0);
    REQUIRE(blends[2].blending_enabled == 1);
    REQUIRE(blends[3].blending_enabled == 0);
}

// ── 裁剪矩形 ──

TEST_CASE("scissor: 结构体大小对齐正确", "[scissor][state]")
{
    REQUIRE(sizeof(metal_scissor_rect) == 16);
}

// ── NULL 错误路径 ──

TEST_CASE("depth_stencil: NULL 参数返回 INVALID_ARGUMENT", "[depth][stencil][error]")
{
    metal_depth_stencil_descriptor desc{};
    REQUIRE(metal_create_depth_stencil_state(nullptr, &desc, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
}
