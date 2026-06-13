// test_sampler.cpp — P4.1.4 MetalSampler 单元测试
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include "metal_bridge.h"

#include <cstring>

TEST_CASE("采样器创建 — 默认参数", "[sampler]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_sampler_descriptor desc = {};
    desc.mag_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.min_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.mip_filter = METAL_SAMPLER_MIP_FILTER_NEAREST;
    desc.address_s = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    desc.address_t = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    desc.address_r = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    desc.compare_function = METAL_COMPARE_ALWAYS;
    desc.max_anisotropy = 1.0f;
    desc.lod_min_clamp = 0.0f;
    desc.lod_max_clamp = 0.0f;
    desc.normalized_coordinates = true;

    metal_sampler* sampler = nullptr;
    REQUIRE(metal_create_sampler(device, &desc, &sampler) == METAL_RESULT_OK);
    REQUIRE(sampler != nullptr);

    metal_release(sampler);
    metal_release(device);
}

TEST_CASE("采样器创建 — 各向异性 16x", "[sampler]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_sampler_descriptor desc = {};
    desc.mag_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.min_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.mip_filter = METAL_SAMPLER_MIP_FILTER_LINEAR;
    desc.address_s = METAL_SAMPLER_ADDRESS_REPEAT;
    desc.address_t = METAL_SAMPLER_ADDRESS_REPEAT;
    desc.address_r = METAL_SAMPLER_ADDRESS_REPEAT;
    desc.compare_function = METAL_COMPARE_ALWAYS;
    desc.max_anisotropy = 16.0f;
    desc.lod_min_clamp = 0.0f;
    desc.lod_max_clamp = 10.0f;
    desc.normalized_coordinates = true;

    metal_sampler* sampler = nullptr;
    REQUIRE(metal_create_sampler(device, &desc, &sampler) == METAL_RESULT_OK);
    REQUIRE(sampler != nullptr);

    metal_release(sampler);
    metal_release(device);
}

TEST_CASE("采样器创建 — 阴影比较采样器", "[sampler]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_sampler_descriptor desc = {};
    desc.mag_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.min_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.mip_filter = METAL_SAMPLER_MIP_FILTER_NEAREST;
    desc.address_s = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    desc.address_t = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    desc.address_r = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
    desc.compare_function = METAL_COMPARE_LESS;
    desc.max_anisotropy = 1.0f;
    desc.lod_min_clamp = 0.0f;
    desc.lod_max_clamp = 0.0f;
    desc.normalized_coordinates = true;

    metal_sampler* sampler = nullptr;
    REQUIRE(metal_create_sampler(device, &desc, &sampler) == METAL_RESULT_OK);
    REQUIRE(sampler != nullptr);

    metal_release(sampler);
    metal_release(device);
}

TEST_CASE("采样器创建 — 边界颜色寻址", "[sampler]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_sampler_descriptor desc = {};
    desc.mag_filter = METAL_SAMPLER_FILTER_NEAREST;
    desc.min_filter = METAL_SAMPLER_FILTER_NEAREST;
    desc.mip_filter = METAL_SAMPLER_MIP_FILTER_NOT_MIPMAPPED;
    desc.address_s = METAL_SAMPLER_ADDRESS_CLAMP_TO_BORDER_COLOR;
    desc.address_t = METAL_SAMPLER_ADDRESS_CLAMP_TO_BORDER_COLOR;
    desc.address_r = METAL_SAMPLER_ADDRESS_CLAMP_TO_BORDER_COLOR;
    desc.compare_function = METAL_COMPARE_NEVER;
    desc.max_anisotropy = 1.0f;
    desc.lod_min_clamp = 0.0f;
    desc.lod_max_clamp = 0.0f;
    desc.normalized_coordinates = true;

    metal_sampler* sampler = nullptr;
    REQUIRE(metal_create_sampler(device, &desc, &sampler) == METAL_RESULT_OK);
    REQUIRE(sampler != nullptr);

    metal_release(sampler);
    metal_release(device);
}

TEST_CASE("采样器创建 — MirrorClampToEdge 寻址", "[sampler]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_sampler_descriptor desc = {};
    desc.mag_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.min_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.mip_filter = METAL_SAMPLER_MIP_FILTER_LINEAR;
    desc.address_s = METAL_SAMPLER_ADDRESS_MIRROR_CLAMP_TO_EDGE;
    desc.address_t = METAL_SAMPLER_ADDRESS_MIRROR_CLAMP_TO_EDGE;
    desc.address_r = METAL_SAMPLER_ADDRESS_MIRROR_CLAMP_TO_EDGE;
    desc.compare_function = METAL_COMPARE_ALWAYS;
    desc.max_anisotropy = 4.0f;
    desc.lod_min_clamp = -2.0f;
    desc.lod_max_clamp = 8.0f;
    desc.normalized_coordinates = false;

    metal_sampler* sampler = nullptr;
    REQUIRE(metal_create_sampler(device, &desc, &sampler) == METAL_RESULT_OK);
    REQUIRE(sampler != nullptr);

    metal_release(sampler);
    metal_release(device);
}

TEST_CASE("采样器创建 — 空参数返回 INVALID_ARGUMENT", "[sampler]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_sampler* sampler = nullptr;

    // NULL device
    REQUIRE(metal_create_sampler(nullptr, nullptr, &sampler) == METAL_RESULT_INVALID_ARGUMENT);

    // NULL descriptor
    REQUIRE(metal_create_sampler(device, nullptr, &sampler) == METAL_RESULT_INVALID_ARGUMENT);

    // NULL out
    metal_sampler_descriptor desc = {};
    desc.mag_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.min_filter = METAL_SAMPLER_FILTER_LINEAR;
    REQUIRE(metal_create_sampler(device, &desc, nullptr) == METAL_RESULT_INVALID_ARGUMENT);

    metal_release(device);
}

TEST_CASE("采样器创建 — 多种过滤器组合", "[sampler]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_sampler_min_mag_filter mag_filters[] = {
        METAL_SAMPLER_FILTER_NEAREST,
        METAL_SAMPLER_FILTER_LINEAR,
    };
    metal_sampler_min_mag_filter min_filters[] = {
        METAL_SAMPLER_FILTER_NEAREST,
        METAL_SAMPLER_FILTER_LINEAR,
    };
    metal_sampler_mip_filter mip_filters[] = {
        METAL_SAMPLER_MIP_FILTER_NOT_MIPMAPPED,
        METAL_SAMPLER_MIP_FILTER_NEAREST,
        METAL_SAMPLER_MIP_FILTER_LINEAR,
    };

    for (auto mag : mag_filters)
    {
        for (auto min : min_filters)
        {
            for (auto mip : mip_filters)
            {
                metal_sampler_descriptor desc = {};
                desc.mag_filter = mag;
                desc.min_filter = min;
                desc.mip_filter = mip;
                desc.address_s = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
                desc.address_t = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
                desc.address_r = METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE;
                desc.compare_function = METAL_COMPARE_ALWAYS;
                desc.max_anisotropy = 1.0f;
                desc.lod_min_clamp = 0.0f;
                desc.lod_max_clamp = 0.0f;
                desc.normalized_coordinates = true;

                metal_sampler* sampler = nullptr;
                REQUIRE(metal_create_sampler(device, &desc, &sampler) == METAL_RESULT_OK);
                REQUIRE(sampler != nullptr);
                metal_release(sampler);
            }
        }
    }

    metal_release(device);
}

TEST_CASE("采样器创建 — 释放后 device 仍然可用", "[sampler]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_sampler_descriptor desc = {};
    desc.mag_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.min_filter = METAL_SAMPLER_FILTER_LINEAR;
    desc.mip_filter = METAL_SAMPLER_MIP_FILTER_NEAREST;
    desc.address_s = METAL_SAMPLER_ADDRESS_REPEAT;
    desc.address_t = METAL_SAMPLER_ADDRESS_REPEAT;
    desc.address_r = METAL_SAMPLER_ADDRESS_REPEAT;
    desc.max_anisotropy = 1.0f;
    desc.lod_min_clamp = 0.0f;
    desc.lod_max_clamp = 0.0f;
    desc.normalized_coordinates = true;

    metal_sampler* sampler = nullptr;
    REQUIRE(metal_create_sampler(device, &desc, &sampler) == METAL_RESULT_OK);
    REQUIRE(sampler != nullptr);

    // 释放采样器后 device 不应受影响
    metal_release(sampler);

    // 再次创建采样器确认 device 仍然可用
    sampler = nullptr;
    REQUIRE(metal_create_sampler(device, &desc, &sampler) == METAL_RESULT_OK);
    REQUIRE(sampler != nullptr);
    metal_release(sampler);

    metal_release(device);
}
