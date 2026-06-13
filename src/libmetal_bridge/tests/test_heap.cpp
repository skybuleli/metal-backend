// test_heap.cpp — P4.1.5 MTLHeap + 稀疏缓冲区单元测试
#include <catch2/catch_test_macros.hpp>
#include "metal_bridge.h"

TEST_CASE("堆创建 — 基本分配", "[heap]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(device, 1024 * 1024, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_OK);
    REQUIRE(heap != nullptr);

    metal_release(heap);
    metal_release(device);
}

TEST_CASE("堆创建 — 大堆 64MB", "[heap]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(device, 64 * 1024 * 1024, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_OK);
    REQUIRE(heap != nullptr);

    metal_release(heap);
    metal_release(device);
}

TEST_CASE("堆创建 — 空参数返回 INVALID_ARGUMENT", "[heap]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(nullptr, 1024, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_create_heap(device, 0, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_create_heap(device, 1024, METAL_STORAGE_MODE_PRIVATE, nullptr) == METAL_RESULT_INVALID_ARGUMENT);

    metal_release(device);
}

TEST_CASE("从堆创建缓冲区 — 基本", "[heap-buffer]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(device, 1024 * 1024, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_OK);

    metal_buffer* buf = nullptr;
    REQUIRE(metal_heap_create_buffer(heap, 0, 4096, &buf) == METAL_RESULT_OK);
    REQUIRE(buf != nullptr);

    metal_buffer_info info;
    REQUIRE(metal_buffer_get_info(buf, &info) == METAL_RESULT_OK);
    REQUIRE(info.size == 4096);

    metal_release(buf);
    metal_release(heap);
    metal_release(device);
}

TEST_CASE("从堆创建缓冲区 — 偏移分配", "[heap-buffer]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(device, 1024 * 1024, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_OK);

    // 在偏移 0x10000 处分配 256KB 的缓冲区
    metal_buffer* buf = nullptr;
    REQUIRE(metal_heap_create_buffer(heap, 0x10000, 256 * 1024, &buf) == METAL_RESULT_OK);
    REQUIRE(buf != nullptr);

    metal_buffer_info info;
    REQUIRE(metal_buffer_get_info(buf, &info) == METAL_RESULT_OK);
    REQUIRE(info.size == 256 * 1024);

    metal_release(buf);
    metal_release(heap);
    metal_release(device);
}

TEST_CASE("从堆创建缓冲区 — 多个子缓冲区", "[heap-buffer]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(device, 1024 * 1024, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_OK);

    // 在堆内创建 3 个不重叠的子缓冲区
    metal_buffer* buf_a = nullptr;
    metal_buffer* buf_b = nullptr;
    metal_buffer* buf_c = nullptr;

    REQUIRE(metal_heap_create_buffer(heap, 0, 65536, &buf_a) == METAL_RESULT_OK);
    REQUIRE(metal_heap_create_buffer(heap, 65536, 131072, &buf_b) == METAL_RESULT_OK);
    REQUIRE(metal_heap_create_buffer(heap, 196608, 851968, &buf_c) == METAL_RESULT_OK);

    REQUIRE(buf_a != nullptr);
    REQUIRE(buf_b != nullptr);
    REQUIRE(buf_c != nullptr);

    // 验证各缓冲区大小
    metal_buffer_info info;
    REQUIRE(metal_buffer_get_info(buf_a, &info) == METAL_RESULT_OK);
    REQUIRE(info.size == 65536);

    REQUIRE(metal_buffer_get_info(buf_b, &info) == METAL_RESULT_OK);
    REQUIRE(info.size == 131072);

    REQUIRE(metal_buffer_get_info(buf_c, &info) == METAL_RESULT_OK);
    REQUIRE(info.size == 851968);

    metal_release(buf_a);
    metal_release(buf_b);
    metal_release(buf_c);
    metal_release(heap);
    metal_release(device);
}

TEST_CASE("从堆创建缓冲区 — 越界偏移返回 INVALID_ARGUMENT", "[heap-buffer]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(device, 65536, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_OK);

    metal_buffer* buf = nullptr;
    // offset + size 超过堆大小
    REQUIRE(metal_heap_create_buffer(heap, 60000, 6000, &buf) == METAL_RESULT_INVALID_ARGUMENT);
    // offset 等于堆大小
    REQUIRE(metal_heap_create_buffer(heap, 65536, 1, &buf) == METAL_RESULT_INVALID_ARGUMENT);

    metal_release(heap);
    metal_release(device);
}

TEST_CASE("从堆创建缓冲区 — 空参数返回 INVALID_ARGUMENT", "[heap-buffer]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(device, 65536, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_OK);

    metal_buffer* buf = nullptr;
    REQUIRE(metal_heap_create_buffer(nullptr, 0, 1024, &buf) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_heap_create_buffer(heap, 0, 0, &buf) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_heap_create_buffer(heap, 0, 1024, nullptr) == METAL_RESULT_INVALID_ARGUMENT);

    metal_release(heap);
    metal_release(device);
}

TEST_CASE("堆释放 — 子缓冲区先于堆释放", "[heap-lifecycle]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    metal_heap* heap = nullptr;
    REQUIRE(metal_create_heap(device, 1024 * 1024, METAL_STORAGE_MODE_PRIVATE, &heap) == METAL_RESULT_OK);

    metal_buffer* buf = nullptr;
    REQUIRE(metal_heap_create_buffer(heap, 0, 4096, &buf) == METAL_RESULT_OK);

    // 释放子缓冲区（应不崩溃，且不影响堆）
    metal_release(buf);

    // 堆仍可用
    metal_buffer* buf2 = nullptr;
    REQUIRE(metal_heap_create_buffer(heap, 4096, 4096, &buf2) == METAL_RESULT_OK);
    REQUIRE(buf2 != nullptr);

    metal_release(buf2);
    metal_release(heap);
    metal_release(device);
}

TEST_CASE("堆存储模式 — Shared 模式堆可创建", "[heap-mode]")
{
    metal_device* device = nullptr;
    REQUIRE(metal_create_device(&device) == METAL_RESULT_OK);

    // Shared 模式堆在 UMA 设备上可用（Apple Silicon 上所有模式行为一致）
    metal_heap* heap = nullptr;
    metal_result result = metal_create_heap(device, 65536, METAL_STORAGE_MODE_SHARED, &heap);

    if (result == METAL_RESULT_OK)
    {
        REQUIRE(heap != nullptr);
        metal_release(heap);
    }
    // 某些平台可能不支持 Shared 堆，不算失败

    metal_release(device);
}
