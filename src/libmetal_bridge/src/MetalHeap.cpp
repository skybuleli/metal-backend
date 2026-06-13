// MetalHeap.cpp — Metal 堆管理：MTLHeap 创建 + 子缓冲区分配
//
// P4.1.5 稀疏缓冲区实现。通过 MTLHeap 实现多个 MTLBuffer 共享
// 同一块物理内存，支持 offset+size 的稀疏资源视图。
//
// 设计原则：
//   1. metal_create_heap 创建 MTLHeap（Private 模式，GPU 最优）
//   2. metal_heap_create_buffer 从 heap 分配子缓冲区
//   3. 子缓冲区的生命周期绑定到 heap：释放 heap 时自动释放所有子 buffer
//   4. metal_release 统一析构路径支持 heap 类型

#include "metal_bridge.h"
#include "metal_internal.h"

#include <new>

// ════════════════════════════════════════════════════════════════════
// 内部辅助：存储模式 → MTLResourceOptions
// ════════════════════════════════════════════════════════════════════

static MTL::ResourceOptions to_resource_options(metal_storage_mode mode)
{
    switch (mode)
    {
    case METAL_STORAGE_MODE_SHARED:    return MTL::ResourceStorageModeShared;
    case METAL_STORAGE_MODE_MANAGED:   return MTL::ResourceStorageModeManaged;
    case METAL_STORAGE_MODE_PRIVATE:   return MTL::ResourceStorageModePrivate;
    case METAL_STORAGE_MODE_MEMORYLESS:return MTL::ResourceStorageModeMemoryless;
    default:                           return MTL::ResourceStorageModePrivate;
    }
}

// ════════════════════════════════════════════════════════════════════
// P4.1.5 — metal_create_heap
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_heap(
    metal_device* device,
    uint64_t size,
    metal_storage_mode mode,
    metal_heap** out_heap)
{
    if (device == nullptr || out_heap == nullptr || size == 0)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    // 构建 MTLHeapDescriptor
    MTL::HeapDescriptor* heap_desc = MTL::HeapDescriptor::alloc()->init();
    if (heap_desc == nullptr)
    {
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    heap_desc->setSize(static_cast<NS::UInteger>(size));
    heap_desc->setStorageMode(to_resource_options(mode));

    // HeapType 使用 Automatic 让 Metal 自动管理放置策略
    // 在 Apple Silicon 上 Placement 和 Automatic 行为一致
    heap_desc->setType(MTL::HeapTypeAutomatic);

    MTL::Heap* mtl_heap = device->device->newHeap(heap_desc);
    heap_desc->release();

    if (mtl_heap == nullptr)
    {
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    metal_heap* heap = new (std::nothrow) metal_heap();
    if (heap == nullptr)
    {
        mtl_heap->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    heap->base.type = METAL_HANDLE_TYPE_HEAP;
    heap->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    heap->heap = mtl_heap;
    heap->size = static_cast<size_t>(size);

    pool->release();
    *out_heap = heap;
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// P4.1.5 — metal_heap_create_buffer
// ════════════════════════════════════════════════════════════════════

metal_result metal_heap_create_buffer(
    metal_heap* heap,
    uint64_t offset,
    uint64_t size,
    metal_buffer** out_buffer)
{
    if (heap == nullptr || out_buffer == nullptr || size == 0)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (heap->base.type != METAL_HANDLE_TYPE_HEAP)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (heap->heap == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // 边界检查
    if (offset + size > static_cast<uint64_t>(heap->size))
        return METAL_RESULT_INVALID_ARGUMENT;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::Buffer* mtl_buf = heap->heap->newBuffer(
        static_cast<NS::UInteger>(size),
        MTL::ResourceStorageModePrivate,
        static_cast<NS::UInteger>(offset));

    if (mtl_buf == nullptr)
    {
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    metal_buffer* buf = new (std::nothrow) metal_buffer();
    if (buf == nullptr)
    {
        mtl_buf->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    buf->base.type = METAL_HANDLE_TYPE_BUFFER;
    buf->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    buf->buffer = mtl_buf;
    buf->size = static_cast<size_t>(size);
    buf->mode = METAL_STORAGE_MODE_PRIVATE;  // heap 子缓冲区总是 Private

    pool->release();
    *out_buffer = buf;
    return METAL_RESULT_OK;
}
