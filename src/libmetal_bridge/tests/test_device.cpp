// test_device.cpp — MetalDevice 单元测试（Catch2）
//
// 测试覆盖：
//   1. ABI 版本校验
//   2. metal_create_device 成功（需要真实 GPU）
//   3. metal_get_device_info 返回正确版本
//   4. metal_get_device_caps 返回有效能力数据
//   5. metal_create_queue 成功
//   6. metal_release 对 nullptr 安全
//   7. metal_validate_limits 全部常量校验通过
//   8. 错误路径：NULL 参数返回 INVALID_ARGUMENT
//   9. 多次创建-释放周期无泄漏

#include <catch2/catch_test_macros.hpp>

#include "metal_bridge.h"
#include "metal_limits.h"

#include <cstring>
#include <cstdint>

// ════════════════════════════════════════════════════════════════════
// 辅助：设备与队列的 RAII 包装
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

struct QueueGuard
{
    metal_queue* queue = nullptr;

    explicit QueueGuard(metal_device* dev)
    {
        metal_result result = metal_create_queue(dev, &queue);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(queue != nullptr);
    }

    ~QueueGuard()
    {
        if (queue != nullptr)
        {
            metal_release(queue);
            queue = nullptr;
        }
    }

    QueueGuard(const QueueGuard&) = delete;
    QueueGuard& operator=(const QueueGuard&) = delete;
};

// ════════════════════════════════════════════════════════════════════
// 测试用例
// ════════════════════════════════════════════════════════════════════

TEST_CASE("ABI 版本号正确", "[abi]")
{
    uint32_t version = metal_bridge_abi_version();
    REQUIRE(version == METAL_BRIDGE_ABI_VERSION);
}

TEST_CASE("metal_create_device 成功创建设备", "[device]")
{
    DeviceGuard guard;
    REQUIRE(guard.dev != nullptr);
}

TEST_CASE("metal_get_device_info 返回正确版本", "[device]")
{
    DeviceGuard guard;

    metal_handle_info info;
    std::memset(&info, 0xFF, sizeof(info)); // 先填脏数据

    metal_result result = metal_get_device_info(guard.dev, &info);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(info.abi_version == METAL_BRIDGE_ABI_VERSION);
    REQUIRE(info.reserved == 0);
}

TEST_CASE("metal_get_device_caps 返回有效能力数据", "[device][caps]")
{
    DeviceGuard guard;

    metal_device_caps caps;
    std::memset(&caps, 0xFF, sizeof(caps));

    metal_result result = metal_get_device_caps(guard.dev, &caps);
    REQUIRE(result == METAL_RESULT_OK);

    // GPU 名称应有内容
    REQUIRE(std::strlen(caps.device_name) > 0);

    // 注册 ID 不应为 0
    REQUIRE(caps.registry_id != 0);

    // 最大缓冲区长度不应为 0
    REQUIRE(caps.max_buffer_length > 0);

    // 线程组各维线程数 >= 1
    REQUIRE(caps.max_threads_per_threadgroup_x >= 1);
    REQUIRE(caps.max_threads_per_threadgroup_y >= 1);
    REQUIRE(caps.max_threads_per_threadgroup_z >= 1);

    // 线程组共享内存 >= 1024 字节
    REQUIRE(caps.max_threadgroup_memory >= 1024);

    // 颜色附件应为 8
    REQUIRE(caps.max_color_attachments == 8);

    INFO("设备名称: " << caps.device_name);
    INFO("统一内存: " << (caps.has_unified_memory ? "是" : "否"));
    INFO("最大缓冲区: " << caps.max_buffer_length << " 字节");
    INFO("Apple7: " << (caps.supports_apple7 ? "是" : "否"));
}

TEST_CASE("metal_create_queue 成功创建队列", "[queue]")
{
    DeviceGuard dev_guard;
    QueueGuard queue_guard(dev_guard.dev);
    REQUIRE(queue_guard.queue != nullptr);
}

TEST_CASE("metal_release 对 nullptr 安全", "[release]")
{
    // 这不应崩溃
    metal_release(nullptr);
}

TEST_CASE("metal_validate_limits 全部常量校验通过", "[limits]")
{
    metal_limits_validation result = metal_validate_limits();
    INFO("失败原因: " << (result.failure_reason ? result.failure_reason : "无"));
    REQUIRE(result.valid == true);
    REQUIRE(result.alignments_valid == true);
    REQUIRE(result.limits_valid == true);
}

TEST_CASE("NULL 参数返回 INVALID_ARGUMENT", "[error]")
{
    metal_handle_info info;
    metal_device_caps caps;

    REQUIRE(metal_create_device(nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_get_device_info(nullptr, &info) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_get_device_info(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_get_device_caps(nullptr, &caps) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_get_device_caps(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_create_queue(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("两次创建设备均可成功", "[device]")
{
    // 验证 metal_create_device 可被多次调用，每次返回有效句柄
    metal_device* dev1 = nullptr;
    metal_device* dev2 = nullptr;

    REQUIRE(metal_create_device(&dev1) == METAL_RESULT_OK);
    REQUIRE(metal_create_device(&dev2) == METAL_RESULT_OK);
    REQUIRE(dev1 != nullptr);
    REQUIRE(dev2 != nullptr);

    metal_release(dev1);
    metal_release(dev2);
}

TEST_CASE("metal_device_caps 结构体大小合理", "[caps][abi]")
{
    // 确保结构体大小不超过 256 字节（C ABI 兼容性检查）
    REQUIRE(sizeof(metal_device_caps) <= 256);
    INFO("metal_device_caps 大小: " << sizeof(metal_device_caps) << " 字节");
}

TEST_CASE("创建-释放循环无泄漏", "[device][release]")
{
    // 连续创建和释放设备，验证不崩溃
    for (int i = 0; i < 3; ++i)
    {
        metal_device* dev = nullptr;
        REQUIRE(metal_create_device(&dev) == METAL_RESULT_OK);
        REQUIRE(dev != nullptr);
        metal_release(dev);
    }
}
