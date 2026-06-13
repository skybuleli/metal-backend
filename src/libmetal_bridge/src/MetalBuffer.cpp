// MetalBuffer.cpp — Metal 缓冲区管理：创建、映射、刷回、信息查询
//
// 本文件严格遵循以下设计原则：
//   1. 不定义 NS_PRIVATE_IMPLEMENTATION / MTL_PRIVATE_IMPLEMENTATION
//      （已在 MetalDevice.cpp 中定义，重复定义会报错）
//   2. 所有 metal-cpp 属性访问器和工厂方法调用必须在
//      NS::AutoreleasePool 作用域内
//   3. 所有指针参数检查返回 METAL_RESULT_INVALID_ARGUMENT
//
// 注意：metal_device 是 opaque 类型（在 metal_bridge.h 中前向声明，
// 完整定义仅在 MetalDevice.cpp 中）。本文件不访问其内部成员，
// 仅通过指针传递。存储模式策略由 C# 调用方决定（C# 侧可通过
// metal_get_device_caps 获得 hasUnifiedMemory 信息并使用
// METAL_UMA_DEFAULT_STORAGE / METAL_DISCRETE_CPU_VISIBLE_STORAGE）。

#include "metal_bridge.h"
#include "metal_limits.h"
#include "metal_internal.h"

#include <cstring>
#include <cstdint>
#include <new>

// ════════════════════════════════════════════════════════════════════
// 内部辅助函数
// ════════════════════════════════════════════════════════════════════

/// 将 metal_storage_mode 枚举转为 MTL::ResourceOptions
static MTL::ResourceOptions to_resource_options(metal_storage_mode mode)
{
    switch (mode)
    {
    case METAL_STORAGE_MODE_SHARED:
        return MTL::ResourceStorageModeShared;
    case METAL_STORAGE_MODE_MANAGED:
        return MTL::ResourceStorageModeManaged;
    case METAL_STORAGE_MODE_PRIVATE:
        return MTL::ResourceStorageModePrivate;
    case METAL_STORAGE_MODE_MEMORYLESS:
        return MTL::ResourceStorageModeMemoryless;
    default:
        return MTL::ResourceStorageModeShared; // 默认回退
    }
}

/// 分配并初始化 metal_buffer 内部结构体
/// 注意：mtl_buffer 的所有权已转移至此函数，失败时由本函数释放
static metal_buffer* allocate_buffer_struct(
    MTL::Buffer* mtl_buffer,
    size_t size,
    metal_storage_mode mode,
    metal_result* out_result)
{
    if (mtl_buffer == nullptr)
    {
        *out_result = METAL_RESULT_OUT_OF_MEMORY;
        return nullptr;
    }

    metal_buffer* buf = new (std::nothrow) metal_buffer();
    if (buf == nullptr)
    {
        mtl_buffer->release();
        *out_result = METAL_RESULT_OUT_OF_MEMORY;
        return nullptr;
    }

    buf->base.type = METAL_HANDLE_TYPE_BUFFER;
    buf->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    buf->buffer = mtl_buffer;
    buf->size = size;
    buf->mode = mode;

    *out_result = METAL_RESULT_OK;
    return buf;
}

// ════════════════════════════════════════════════════════════════════
// 创建缓冲区
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_buffer(
    metal_device* device,
    uint64_t size,
    metal_storage_mode mode,
    metal_buffer** out_buffer)
{
    if (device == nullptr || out_buffer == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)  // 通过 metal_internal.h 可直接访问
        return METAL_RESULT_RUNTIME_ERROR;

    if (size == 0)
        size = METAL_MIN_CONSTANT_BUFFER_SIZE;

    // 确保大小向上对齐到 256 字节（Metal 缓冲区对齐要求）
    size = METAL_ALIGN_UP(static_cast<size_t>(size), METAL_BUFFER_OFFSET_ALIGNMENT);

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::ResourceOptions options = to_resource_options(mode);

    MTL::Buffer* mtl_buffer = device->device->newBuffer(
        static_cast<NS::UInteger>(size),
        options);

    metal_result result;
    metal_buffer* buf = allocate_buffer_struct(mtl_buffer, size, mode, &result);
    pool->release();

    if (buf == nullptr)
        return result;

    *out_buffer = buf;
    return METAL_RESULT_OK;
}

metal_result metal_create_buffer_with_bytes(
    metal_device* device,
    const void* data,
    uint64_t size,
    metal_storage_mode mode,
    metal_buffer** out_buffer)
{
    if (device == nullptr || out_buffer == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    if (data == nullptr && size > 0)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (size == 0)
        size = METAL_MIN_CONSTANT_BUFFER_SIZE;

    // 确保大小向上对齐
    size = METAL_ALIGN_UP(static_cast<size_t>(size), METAL_BUFFER_OFFSET_ALIGNMENT);

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::ResourceOptions options = to_resource_options(mode);

    MTL::Buffer* mtl_buffer = device->device->newBuffer(
        data,
        static_cast<NS::UInteger>(size),
        options);

    metal_result result;
    metal_buffer* buf = allocate_buffer_struct(mtl_buffer, size, mode, &result);
    pool->release();

    if (buf == nullptr)
        return result;

    *out_buffer = buf;
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// 缓冲区信息查询
// ════════════════════════════════════════════════════════════════════

metal_result metal_buffer_get_info(
    metal_buffer* buffer,
    metal_buffer_info* out_info)
{
    if (buffer == nullptr || out_info == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->base.type != METAL_HANDLE_TYPE_BUFFER)
        return METAL_RESULT_INVALID_ARGUMENT;

    out_info->size = static_cast<uint64_t>(buffer->size);
    out_info->storage_mode = buffer->mode;
    out_info->reserved = 0;
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// 缓冲区映射 / 解除映射
// ════════════════════════════════════════════════════════════════════

metal_result metal_map_buffer(
    metal_buffer* buffer,
    void** out_ptr)
{
    if (buffer == nullptr || out_ptr == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->base.type != METAL_HANDLE_TYPE_BUFFER)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->buffer == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    if (buffer->mode == METAL_STORAGE_MODE_PRIVATE ||
        buffer->mode == METAL_STORAGE_MODE_MEMORYLESS)
    {
        // Private 和 Memoryless 模式的缓冲区 CPU 不可见
        *out_ptr = nullptr;
        return METAL_RESULT_UNSUPPORTED;
    }

    *out_ptr = buffer->buffer->contents();

    // Managed 模式需要在读取前同步
    if (buffer->mode == METAL_STORAGE_MODE_MANAGED)
    {
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        buffer->buffer->didModifyRange(
            NS::Range::Make(0, static_cast<NS::UInteger>(buffer->size)));
        pool->release();
    }

    return METAL_RESULT_OK;
}

metal_result metal_unmap_buffer(
    metal_buffer* buffer)
{
    if (buffer == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->base.type != METAL_HANDLE_TYPE_BUFFER)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->buffer == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // Shared 模式无需解除映射；Private/Memoryless 不可映射
    // Managed 模式：CPU 写完后需要同步到 GPU
    if (buffer->mode == METAL_STORAGE_MODE_MANAGED)
    {
        NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
        buffer->buffer->didModifyRange(
            NS::Range::Make(0, static_cast<NS::UInteger>(buffer->size)));
        pool->release();
    }

    return METAL_RESULT_OK;
}

metal_result metal_flush_buffer(
    metal_buffer* buffer,
    uint64_t offset,
    uint64_t size)
{
    if (buffer == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->base.type != METAL_HANDLE_TYPE_BUFFER)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->buffer == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // 只有 Managed 模式需要显式同步
    if (buffer->mode != METAL_STORAGE_MODE_MANAGED)
        return METAL_RESULT_OK;

    // 参数校验
    if (offset >= buffer->size)
        return METAL_RESULT_INVALID_ARGUMENT;

    NS::UInteger flush_offset = static_cast<NS::UInteger>(offset);
    NS::UInteger flush_size;

    if (size == 0 || offset + size > buffer->size)
    {
        flush_size = static_cast<NS::UInteger>(buffer->size - offset);
    }
    else
    {
        flush_size = static_cast<NS::UInteger>(size);
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    buffer->buffer->didModifyRange(NS::Range::Make(flush_offset, flush_size));
    pool->release();

    return METAL_RESULT_OK;
}
