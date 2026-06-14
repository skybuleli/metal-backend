// test_command_buffer.cpp — P4.4.1 命令缓冲区提交与等待回归测试
//
// 测试目标：
//   1. metal_begin_command_buffer 能返回有效句柄
//   2. metal_commit_command_buffer 能提交空命令缓冲区
//   3. metal_wait_command_buffer 能等待已提交命令缓冲区完成
//   4. NULL 参数返回 INVALID_ARGUMENT

#include <catch2/catch_test_macros.hpp>

#include "metal_bridge.h"

TEST_CASE("命令缓冲区提交与等待链路可用", "[command-buffer][submit][wait]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);
    REQUIRE(device != nullptr);

    metal_queue* queue = nullptr;
    REQUIRE(metal_create_queue(device, &queue) == METAL_RESULT_OK);
    REQUIRE(queue != nullptr);

    metal_command_buffer* commandBuffer = nullptr;
    REQUIRE(metal_begin_command_buffer(queue, &commandBuffer) == METAL_RESULT_OK);
    REQUIRE(commandBuffer != nullptr);

    REQUIRE(metal_commit_command_buffer(commandBuffer) == METAL_RESULT_OK);
    REQUIRE(metal_wait_command_buffer(commandBuffer) == METAL_RESULT_OK);

    metal_release(commandBuffer);
    metal_release(queue);
    metal_release(device);
}

TEST_CASE("命令缓冲区空参数返回 INVALID_ARGUMENT", "[command-buffer][error]")
{
    metal_command_buffer* commandBuffer = nullptr;

    REQUIRE(metal_begin_command_buffer(nullptr, &commandBuffer) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_begin_command_buffer(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_commit_command_buffer(nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_wait_command_buffer(nullptr) == METAL_RESULT_INVALID_ARGUMENT);
}
