// test_sync.cpp — P4.4.2 SharedEvent 同步回归测试
//
// 测试目标：
//   1. metal_create_shared_event 能返回有效句柄
//   2. metal_encode_signal_shared_event 能将事件值编码到命令缓冲区
//   3. 提交并等待后 signaledValue 至少达到目标值
//   4. NULL 参数返回 INVALID_ARGUMENT

#include <catch2/catch_test_macros.hpp>

#include "metal_bridge.h"

TEST_CASE("共享事件 signal 链路可用", "[sync][shared-event]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);
    REQUIRE(device != nullptr);

    metal_queue* queue = nullptr;
    REQUIRE(metal_create_queue(device, &queue) == METAL_RESULT_OK);
    REQUIRE(queue != nullptr);

    metal_shared_event* sharedEvent = nullptr;
    REQUIRE(metal_create_shared_event(device, &sharedEvent) == METAL_RESULT_OK);
    REQUIRE(sharedEvent != nullptr);

    metal_command_buffer* commandBuffer = nullptr;
    REQUIRE(metal_begin_command_buffer(queue, &commandBuffer) == METAL_RESULT_OK);
    REQUIRE(commandBuffer != nullptr);

    constexpr uint64_t kSignalValue = 1;
    REQUIRE(metal_encode_signal_shared_event(commandBuffer, sharedEvent, kSignalValue) == METAL_RESULT_OK);
    REQUIRE(metal_commit_command_buffer(commandBuffer) == METAL_RESULT_OK);
    REQUIRE(metal_wait_command_buffer(commandBuffer) == METAL_RESULT_OK);

    uint64_t signaledValue = 0;
    REQUIRE(metal_get_shared_event_signaled_value(sharedEvent, &signaledValue) == METAL_RESULT_OK);
    REQUIRE(signaledValue >= kSignalValue);

    metal_release(commandBuffer);
    metal_release(sharedEvent);
    metal_release(queue);
    metal_release(device);
}

TEST_CASE("共享事件空参数返回 INVALID_ARGUMENT", "[sync][shared-event][error]")
{
    metal_shared_event* sharedEvent = nullptr;
    metal_command_buffer* commandBuffer = nullptr;
    uint64_t signaledValue = 0;

    REQUIRE(metal_create_shared_event(nullptr, &sharedEvent) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_create_shared_event(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_encode_signal_shared_event(commandBuffer, sharedEvent, 1) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_get_shared_event_signaled_value(sharedEvent, &signaledValue) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_get_shared_event_signaled_value(sharedEvent, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("串行提交 10 个 CommandBuffer 按序完成", "[sync][serial][ordering]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);
    REQUIRE(device != nullptr);

    metal_queue* queue = nullptr;
    REQUIRE(metal_create_queue(device, &queue) == METAL_RESULT_OK);
    REQUIRE(queue != nullptr);

    metal_shared_event* sharedEvent = nullptr;
    REQUIRE(metal_create_shared_event(device, &sharedEvent) == METAL_RESULT_OK);
    REQUIRE(sharedEvent != nullptr);

    constexpr int kCount = 10;

    for (int i = 1; i <= kCount; ++i)
    {
        metal_command_buffer* cb = nullptr;
        REQUIRE(metal_begin_command_buffer(queue, &cb) == METAL_RESULT_OK);
        REQUIRE(cb != nullptr);

        REQUIRE(metal_encode_signal_shared_event(cb, sharedEvent, i) == METAL_RESULT_OK);
        REQUIRE(metal_commit_command_buffer(cb) == METAL_RESULT_OK);
        REQUIRE(metal_wait_command_buffer(cb) == METAL_RESULT_OK);

        metal_release(cb);

        // 每步提交后验证信号量值严格递增
        uint64_t signaledValue = 0;
        REQUIRE(metal_get_shared_event_signaled_value(sharedEvent, &signaledValue) == METAL_RESULT_OK);
        REQUIRE(signaledValue == static_cast<uint64_t>(i));
    }

    metal_release(sharedEvent);
    metal_release(queue);
    metal_release(device);
}

TEST_CASE("连续创建→提交→释放 10 个空 CommandBuffer", "[sync][serial][ordering]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);
    REQUIRE(device != nullptr);

    metal_queue* queue = nullptr;
    REQUIRE(metal_create_queue(device, &queue) == METAL_RESULT_OK);
    REQUIRE(queue != nullptr);

    constexpr int kCount = 10;

    for (int i = 0; i < kCount; ++i)
    {
        metal_command_buffer* cb = nullptr;
        REQUIRE(metal_begin_command_buffer(queue, &cb) == METAL_RESULT_OK);
        REQUIRE(cb != nullptr);

        REQUIRE(metal_commit_command_buffer(cb) == METAL_RESULT_OK);
        REQUIRE(metal_wait_command_buffer(cb) == METAL_RESULT_OK);

        metal_release(cb);
    }

    metal_release(queue);
    metal_release(device);
}
