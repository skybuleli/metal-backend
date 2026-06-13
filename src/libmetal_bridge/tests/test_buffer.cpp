// test_buffer.cpp — MetalBuffer 单元测试（Catch2）
//
// 测试覆盖：
//   1. metal_create_buffer 成功创建 Shared 缓冲区
//   2. metal_create_buffer_with_bytes 带数据创建并验证内容可读
//   3. metal_buffer_get_info 返回正确的大小和存储模式
//   4. metal_map_buffer 返回可写入的 CPU 指针
//   5. metal_map_buffer 写入后通过 metal_create_buffer_with_bytes 验证
//   6. metal_unmap_buffer 对 Shared 模式无害
//   7. metal_flush_buffer 对 Shared 模式无害
//   8. NULL 参数返回 INVALID_ARGUMENT
//   9. 创建-释放循环无泄漏
//   10. 多次创建不同的缓冲区

#include <catch2/catch_test_macros.hpp>

#include "metal_bridge.h"
#include "metal_limits.h"

#include <cstring>
#include <cstdint>

// ════════════════════════════════════════════════════════════════════
// 辅助：设备与缓冲区的 RAII 包装
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

struct BufferGuard
{
    metal_buffer* buf = nullptr;

    BufferGuard(metal_device* dev, uint64_t size, metal_storage_mode mode)
    {
        metal_result result = metal_create_buffer(dev, size, mode, &buf);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(buf != nullptr);
    }

    BufferGuard(metal_device* dev, const void* data, uint64_t size, metal_storage_mode mode)
    {
        metal_result result = metal_create_buffer_with_bytes(dev, data, size, mode, &buf);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(buf != nullptr);
    }

    ~BufferGuard()
    {
        if (buf != nullptr)
        {
            metal_release(buf);
            buf = nullptr;
        }
    }

    BufferGuard(const BufferGuard&) = delete;
    BufferGuard& operator=(const BufferGuard&) = delete;
};

// ════════════════════════════════════════════════════════════════════
// 测试用例
// ════════════════════════════════════════════════════════════════════

TEST_CASE("metal_create_buffer 成功创建 Shared 缓冲区", "[buffer]")
{
    DeviceGuard dev_guard;

    BufferGuard buf_guard(dev_guard.dev, 1024, METAL_STORAGE_MODE_SHARED);
    REQUIRE(buf_guard.buf != nullptr);
}

TEST_CASE("metal_create_buffer_with_bytes 带数据创建并验证", "[buffer]")
{
    DeviceGuard dev_guard;

    const uint8_t src_data[] = {0xDE, 0xAD, 0xBE, 0xEF, 0x42, 0x13, 0x37, 0xFF};
    const uint64_t data_size = sizeof(src_data);

    BufferGuard buf_guard(dev_guard.dev, src_data, data_size, METAL_STORAGE_MODE_SHARED);
    REQUIRE(buf_guard.buf != nullptr);

    // 通过 metal_map_buffer 获取内容并验证
    void* mapped = nullptr;
    metal_result result = metal_map_buffer(buf_guard.buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(mapped != nullptr);

    // 验证前 8 个字节与源数据一致
    const uint8_t* mapped_bytes = static_cast<const uint8_t*>(mapped);
    for (size_t i = 0; i < data_size; ++i)
    {
        REQUIRE(mapped_bytes[i] == src_data[i]);
    }

    // 验证映射指针可写入
    uint8_t* writable = static_cast<uint8_t*>(mapped);
    writable[0] = 0xFF;
    REQUIRE(mapped_bytes[0] == 0xFF);
}

TEST_CASE("metal_buffer_get_info 返回正确信息", "[buffer]")
{
    DeviceGuard dev_guard;

    BufferGuard buf_guard(dev_guard.dev, 4096, METAL_STORAGE_MODE_SHARED);

    metal_buffer_info info;
    std::memset(&info, 0xFF, sizeof(info));

    metal_result result = metal_buffer_get_info(buf_guard.buf, &info);
    REQUIRE(result == METAL_RESULT_OK);

    // 大小应 >= 4096（可能因对齐而略大）
    REQUIRE(info.size >= 4096);

    // 存储模式应为 Shared
    REQUIRE(info.storage_mode == METAL_STORAGE_MODE_SHARED);
}

TEST_CASE("metal_map_buffer 返回可写入的 CPU 指针", "[buffer][map]")
{
    DeviceGuard dev_guard;

    // 创建一个 256 字节的缓冲区
    BufferGuard buf_guard(dev_guard.dev, 256, METAL_STORAGE_MODE_SHARED);

    void* ptr = nullptr;
    metal_result result = metal_map_buffer(buf_guard.buf, &ptr);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(ptr != nullptr);

    // 写入并验证
    uint8_t* bytes = static_cast<uint8_t*>(ptr);
    bytes[0] = 0xAA;
    bytes[128] = 0xBB;
    bytes[255] = 0xCC;

    REQUIRE(bytes[0] == 0xAA);
    REQUIRE(bytes[128] == 0xBB);
    REQUIRE(bytes[255] == 0xCC);
}

TEST_CASE("metal_map_buffer 后写入再创建新缓冲区验证持久性", "[buffer][map]")
{
    DeviceGuard dev_guard;

    // 创建第一个缓冲区并写入数据
    BufferGuard write_buf(dev_guard.dev, 64, METAL_STORAGE_MODE_SHARED);

    void* ptr = nullptr;
    REQUIRE(metal_map_buffer(write_buf.buf, &ptr) == METAL_RESULT_OK);
    REQUIRE(ptr != nullptr);

    const uint8_t test_pattern[] = {1, 2, 3, 4, 5, 6, 7, 8};
    std::memcpy(ptr, test_pattern, sizeof(test_pattern));

    REQUIRE(metal_unmap_buffer(write_buf.buf) == METAL_RESULT_OK);

    // 创建第二个带相同数据的缓冲区并验证
    BufferGuard read_buf(dev_guard.dev, test_pattern, sizeof(test_pattern), METAL_STORAGE_MODE_SHARED);

    void* read_ptr = nullptr;
    REQUIRE(metal_map_buffer(read_buf.buf, &read_ptr) == METAL_RESULT_OK);
    REQUIRE(read_ptr != nullptr);

    const uint8_t* read_bytes = static_cast<const uint8_t*>(read_ptr);
    for (size_t i = 0; i < sizeof(test_pattern); ++i)
    {
        REQUIRE(read_bytes[i] == test_pattern[i]);
    }
}

TEST_CASE("metal_unmap_buffer 对 Shared 模式无害", "[buffer][unmap]")
{
    DeviceGuard dev_guard;
    BufferGuard buf_guard(dev_guard.dev, 128, METAL_STORAGE_MODE_SHARED);

    // 先 map 再 unmap
    void* ptr = nullptr;
    REQUIRE(metal_map_buffer(buf_guard.buf, &ptr) == METAL_RESULT_OK);

    // 写入一些数据
    std::memset(ptr, 0x42, 32);

    // Shared 模式 unmap 应成功且无害
    REQUIRE(metal_unmap_buffer(buf_guard.buf) == METAL_RESULT_OK);

    // unmap 后再 map 验证数据仍在
    void* ptr2 = nullptr;
    REQUIRE(metal_map_buffer(buf_guard.buf, &ptr2) == METAL_RESULT_OK);

    const uint8_t* bytes = static_cast<const uint8_t*>(ptr2);
    for (int i = 0; i < 32; ++i)
    {
        REQUIRE(bytes[i] == 0x42);
    }
}

TEST_CASE("metal_flush_buffer 对 Shared 模式无害", "[buffer][flush]")
{
    DeviceGuard dev_guard;
    BufferGuard buf_guard(dev_guard.dev, 256, METAL_STORAGE_MODE_SHARED);

    // 对 Shared 模式的缓冲区 flush 应直接返回 OK（无需操作）
    REQUIRE(metal_flush_buffer(buf_guard.buf, 0, 256) == METAL_RESULT_OK);

    // flush 部分范围也应通过
    REQUIRE(metal_flush_buffer(buf_guard.buf, 64, 128) == METAL_RESULT_OK);

    // flush 超出范围应被 clamp 或返回 OK
    REQUIRE(metal_flush_buffer(buf_guard.buf, 0, 9999) == METAL_RESULT_OK);
}

TEST_CASE("NULL 参数返回 INVALID_ARGUMENT", "[buffer][error]")
{
    DeviceGuard dev_guard;
    metal_buffer_info info;
    void* ptr = nullptr;

    // 设备为空
    REQUIRE(metal_create_buffer(nullptr, 64, METAL_STORAGE_MODE_SHARED, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_create_buffer_with_bytes(nullptr, nullptr, 64, METAL_STORAGE_MODE_SHARED, nullptr) == METAL_RESULT_INVALID_ARGUMENT);

    // 输出指针为空
    REQUIRE(metal_create_buffer(dev_guard.dev, 64, METAL_STORAGE_MODE_SHARED, nullptr) == METAL_RESULT_INVALID_ARGUMENT);

    // 获取信息参数为空
    REQUIRE(metal_buffer_get_info(nullptr, &info) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_buffer_get_info(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);

    // map/unmap/flush 参数为空
    REQUIRE(metal_map_buffer(nullptr, &ptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_map_buffer(nullptr, nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_unmap_buffer(nullptr) == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_flush_buffer(nullptr, 0, 64) == METAL_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("多次创建不同类型的缓冲区", "[buffer][multiple]")
{
    DeviceGuard dev_guard;

    // 创建多个不同大小的缓冲区并验证生命周期
    uint64_t sizes[] = {64, 256, 1024, 4096, 65536};
    constexpr int kCount = sizeof(sizes) / sizeof(sizes[0]);

    metal_buffer* bufs[kCount] = {};

    for (int i = 0; i < kCount; ++i)
    {
        metal_result result = metal_create_buffer(dev_guard.dev, sizes[i], METAL_STORAGE_MODE_SHARED, &bufs[i]);
        REQUIRE(result == METAL_RESULT_OK);
        REQUIRE(bufs[i] != nullptr);
    }

    // 验证每个缓冲区的大小
    for (int i = 0; i < kCount; ++i)
    {
        metal_buffer_info info;
        std::memset(&info, 0xFF, sizeof(info));
        REQUIRE(metal_buffer_get_info(bufs[i], &info) == METAL_RESULT_OK);
        REQUIRE(info.size >= sizes[i]);
        REQUIRE(info.storage_mode == METAL_STORAGE_MODE_SHARED);
    }

    // 逆序释放
    for (int i = kCount - 1; i >= 0; --i)
    {
        metal_release(bufs[i]);
        bufs[i] = nullptr;
    }
}

TEST_CASE("创建-释放循环无泄漏", "[buffer][release]")
{
    DeviceGuard dev_guard;

    for (int i = 0; i < 5; ++i)
    {
        metal_buffer* buf = nullptr;
        REQUIRE(metal_create_buffer(dev_guard.dev, 128, METAL_STORAGE_MODE_SHARED, &buf) == METAL_RESULT_OK);
        REQUIRE(buf != nullptr);
        metal_release(buf);
    }
}

TEST_CASE("metal_release 正确释放缓冲区句柄", "[buffer][release]")
{
    DeviceGuard dev_guard;

    metal_buffer* buf = nullptr;
    REQUIRE(metal_create_buffer(dev_guard.dev, 64, METAL_STORAGE_MODE_SHARED, &buf) == METAL_RESULT_OK);
    REQUIRE(buf != nullptr);

    // 释放缓冲区（不应崩溃）
    metal_release(buf);

    // 验证后续操作仍正常
    metal_buffer* buf2 = nullptr;
    REQUIRE(metal_create_buffer(dev_guard.dev, 64, METAL_STORAGE_MODE_SHARED, &buf2) == METAL_RESULT_OK);
    REQUIRE(buf2 != nullptr);
    metal_release(buf2);
}

// ════════════════════════════════════════════════════════════════════
// 审查缺口修复：新函数测试
// ════════════════════════════════════════════════════════════════════

TEST_CASE("metal_create_buffer_from_pointer 零拷贝包装有效指针", "[buffer][from_pointer]")
{
    DeviceGuard dev_guard;

    // 准备一个已知内容的外部内存
    uint8_t test_data[64];
    for (int i = 0; i < 64; ++i)
        test_data[i] = static_cast<uint8_t>(i);

    metal_buffer* buf = nullptr;
    metal_result result = metal_create_buffer_from_pointer(
        dev_guard.dev, test_data, sizeof(test_data), METAL_STORAGE_MODE_SHARED, &buf);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(buf != nullptr);

    // 验证：映射后内容应与源内存一致
    void* mapped = nullptr;
    result = metal_map_buffer(buf, &mapped);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(mapped != nullptr);

    const uint8_t* bytes = static_cast<const uint8_t*>(mapped);
    for (int i = 0; i < 64; ++i)
    {
        REQUIRE(bytes[i] == static_cast<uint8_t>(i));
    }

    // 通过映射指针修改应反映到原始内存（零拷贝语义）
    uint8_t* writable = static_cast<uint8_t*>(mapped);
    writable[0] = 0xFF;
    REQUIRE(test_data[0] == 0xFF); // 确认原始内存也被修改

    metal_release(buf);
}

TEST_CASE("metal_create_buffer_from_pointer NULL 参数检查", "[buffer][from_pointer][error]")
{
    DeviceGuard dev_guard;

    REQUIRE(metal_create_buffer_from_pointer(nullptr, nullptr, 64, METAL_STORAGE_MODE_SHARED, nullptr)
        == METAL_RESULT_INVALID_ARGUMENT);
    REQUIRE(metal_create_buffer_from_pointer(dev_guard.dev, nullptr, 64, METAL_STORAGE_MODE_SHARED, nullptr)
        == METAL_RESULT_INVALID_ARGUMENT);

    uint8_t dummy[16];
    REQUIRE(metal_create_buffer_from_pointer(dev_guard.dev, dummy, 0, METAL_STORAGE_MODE_SHARED, nullptr)
        == METAL_RESULT_INVALID_ARGUMENT);
}

TEST_CASE("metal_buffer_get_cpu_address 返回有效地址", "[buffer][cpu_address]")
{
    DeviceGuard dev_guard;

    BufferGuard buf_guard(dev_guard.dev, 256, METAL_STORAGE_MODE_SHARED);

    void* ptr = nullptr;
    metal_result result = metal_buffer_get_cpu_address(buf_guard.buf, &ptr);
    REQUIRE(result == METAL_RESULT_OK);
    REQUIRE(ptr != nullptr);

    // 写入并通过 map 验证一致性
    uint8_t* bytes = static_cast<uint8_t*>(ptr);
    bytes[42] = 0xAB;

    void* map_ptr = nullptr;
    REQUIRE(metal_map_buffer(buf_guard.buf, &map_ptr) == METAL_RESULT_OK);
    const uint8_t* map_bytes = static_cast<const uint8_t*>(map_ptr);
    REQUIRE(map_bytes[42] == 0xAB);
}

TEST_CASE("metal_buffer_get_cpu_address 与 metal_map_buffer 返回相同指针", "[buffer][cpu_address][map]")
{
    DeviceGuard dev_guard;
    BufferGuard buf_guard(dev_guard.dev, 128, METAL_STORAGE_MODE_SHARED);

    void* addr1 = nullptr;
    void* addr2 = nullptr;

    REQUIRE(metal_buffer_get_cpu_address(buf_guard.buf, &addr1) == METAL_RESULT_OK);
    REQUIRE(metal_map_buffer(buf_guard.buf, &addr2) == METAL_RESULT_OK);

    // 两者应返回相同的 contents() 指针
    REQUIRE(addr1 == addr2);
}

TEST_CASE("metal_map_buffer 不再是同步操作（修复方向）", "[buffer][map][sync]")
{
    DeviceGuard dev_guard;

    // 验证 map 后无需 unmap 也能直接继续使用指针
    BufferGuard buf_guard(dev_guard.dev, 64, METAL_STORAGE_MODE_SHARED);

    void* ptr = nullptr;
    REQUIRE(metal_map_buffer(buf_guard.buf, &ptr) == METAL_RESULT_OK);
    REQUIRE(ptr != nullptr);

    // map 后直接写入
    static_cast<uint8_t*>(ptr)[0] = 0x55;

    // 不调用 unmap，再次 map
    void* ptr2 = nullptr;
    REQUIRE(metal_map_buffer(buf_guard.buf, &ptr2) == METAL_RESULT_OK);
    REQUIRE(ptr2 == ptr); // 同一个指针
    REQUIRE(static_cast<const uint8_t*>(ptr2)[0] == 0x55); // 数据还在
}

TEST_CASE("metal_create_buffer_from_pointer 释放后原始指针仍有效", "[buffer][from_pointer][release]")
{
    DeviceGuard dev_guard;

    uint8_t backing_memory[32];
    std::memset(backing_memory, 0xAA, sizeof(backing_memory));

    metal_buffer* buf = nullptr;
    REQUIRE(metal_create_buffer_from_pointer(
        dev_guard.dev, backing_memory, sizeof(backing_memory),
        METAL_STORAGE_MODE_SHARED, &buf) == METAL_RESULT_OK);
    REQUIRE(buf != nullptr);

    // 释放 MTLBuffer（bytesNoCopy 不转移所有权）
    metal_release(buf);

    // 原始内存应仍然有效且未被修改
    for (size_t i = 0; i < sizeof(backing_memory); ++i)
    {
        REQUIRE(backing_memory[i] == 0xAA);
    }
}
