// MetalDevice.cpp — Metal 设备管理：创建、能力查询、硬件限制校验
//
// 重要：本文件是 libmetal_bridge 中唯一定义 NS_PRIVATE_IMPLEMENTATION
// 和 MTL_PRIVATE_IMPLEMENTATION 的编译单元。其他 .cpp 文件引用
// Metal.hpp 时不得重复定义这两个宏。

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION

#include "metal_bridge.h"
#include "metal_limits.h"
#include "metal_internal.h"

#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <new>

// ════════════════════════════════════════════════════════════════════
// 内部辅助函数
// ════════════════════════════════════════════════════════════════════

/// 设置设备错误消息
static void set_device_error(metal_device* dev, const char* fmt, ...)
{
    if (dev == nullptr)
        return;

    va_list args;
    va_start(args, fmt);
    std::vsnprintf(dev->error_buf, kErrorBufSize, fmt, args);
    va_end(args);
    dev->error_buf[kErrorBufSize - 1] = '\0';
}

/// 将 NS::String 拷贝到 C 缓冲区
static void copy_ns_string(char* dst, size_t dst_size, NS::String* src)
{
    if (dst == nullptr || dst_size == 0)
        return;
    if (src == nullptr)
    {
        dst[0] = '\0';
        return;
    }
    const char* utf8 = src->utf8String();
    if (utf8 != nullptr)
    {
        std::strncpy(dst, utf8, dst_size - 1);
        dst[dst_size - 1] = '\0';
    }
    else
    {
        dst[0] = '\0';
    }
}

/// 填充设备能力到 caps 结构体
/// 注意：调用者必须在 AutoreleasePool 作用域内调用此函数
static void populate_caps(metal_device* dev)
{
    if (dev == nullptr || dev->device == nullptr)
        return;
    if (dev->caps_populated)
        return;

    MTL::Device* mtl_dev = dev->device;
    metal_device_caps& caps = dev->caps;

    copy_ns_string(caps.device_name, sizeof(caps.device_name), mtl_dev->name());
    caps.has_unified_memory = mtl_dev->hasUnifiedMemory() ? true : false;
    caps.registry_id = static_cast<uint64_t>(mtl_dev->registryID());
    caps.max_buffer_length = static_cast<uint64_t>(mtl_dev->maxBufferLength());

    MTL::Size max_threads = mtl_dev->maxThreadsPerThreadgroup();
    caps.max_threads_per_threadgroup_x = static_cast<uint32_t>(max_threads.width);
    caps.max_threads_per_threadgroup_y = static_cast<uint32_t>(max_threads.height);
    caps.max_threads_per_threadgroup_z = static_cast<uint32_t>(max_threads.depth);
    caps.max_threadgroup_memory = static_cast<uint32_t>(mtl_dev->maxThreadgroupMemoryLength());
    caps.max_argument_buffer_sampler_count = static_cast<uint32_t>(mtl_dev->maxArgumentBufferSamplerCount());
    caps.supports_apple7 = mtl_dev->supportsFamily(MTL::GPUFamilyApple7) ? true : false;
    caps.supports_mac1 = mtl_dev->supportsFamily(MTL::GPUFamilyMac1) ? true : false;
    caps.max_color_attachments = 8u;
    caps.max_viewports = 16u;

    for (int i = 0; i < 8; ++i)
        caps.reserved[i] = 0;

    dev->caps_populated = true;
}

// ════════════════════════════════════════════════════════════════════
// P4.1.0 — 硬件限制常量运行时校验
// ════════════════════════════════════════════════════════════════════

metal_limits_validation metal_validate_limits(void)
{
    metal_limits_validation result;
    result.valid = true;
    result.alignments_valid = true;
    result.limits_valid = true;
    result.failure_reason = nullptr;

    auto is_power_of_2 = [](uint32_t val) -> bool {
        return val != 0u && (val & (val - 1u)) == 0u;
    };

    if (!is_power_of_2(METAL_BUFFER_OFFSET_ALIGNMENT))
    {
        result.alignments_valid = false;
        result.failure_reason = "METAL_BUFFER_OFFSET_ALIGNMENT 不是 2 的幂";
    }
    else if (!is_power_of_2(METAL_CONSTANT_BUFFER_ALIGNMENT))
    {
        result.alignments_valid = false;
        result.failure_reason = "METAL_CONSTANT_BUFFER_ALIGNMENT 不是 2 的幂";
    }
    else if (!is_power_of_2(METAL_TEXTURE_ROW_ALIGNMENT))
    {
        result.alignments_valid = false;
        result.failure_reason = "METAL_TEXTURE_ROW_ALIGNMENT 不是 2 的幂";
    }
    else if (METAL_BUFFER_OFFSET_ALIGNMENT < 4u)
    {
        result.alignments_valid = false;
        result.failure_reason = "METAL_BUFFER_OFFSET_ALIGNMENT 小于 4";
    }

    if (result.alignments_valid)
    {
        if (METAL_MAX_BUFFERS_PER_STAGE == 0u || METAL_MAX_BUFFERS_PER_STAGE > 31u)
        {
            result.limits_valid = false;
            result.failure_reason = "METAL_MAX_BUFFERS_PER_STAGE 超出 [1, 31]";
        }
        else if (METAL_MAX_TEXTURES_PER_STAGE == 0u || METAL_MAX_TEXTURES_PER_STAGE > 128u)
        {
            result.limits_valid = false;
            result.failure_reason = "METAL_MAX_TEXTURES_PER_STAGE 超出 [1, 128]";
        }
        else if (METAL_MAX_SAMPLERS_PER_STAGE == 0u || METAL_MAX_SAMPLERS_PER_STAGE > 16u)
        {
            result.limits_valid = false;
            result.failure_reason = "METAL_MAX_SAMPLERS_PER_STAGE 超出 [1, 16]";
        }
        else if (METAL_MAX_COLOR_ATTACHMENTS == 0u || METAL_MAX_COLOR_ATTACHMENTS > 8u)
        {
            result.limits_valid = false;
            result.failure_reason = "METAL_MAX_COLOR_ATTACHMENTS 超出 [1, 8]";
        }
        else if (METAL_MAX_MSAA_SAMPLES != 1u && METAL_MAX_MSAA_SAMPLES != 2u &&
                 METAL_MAX_MSAA_SAMPLES != 4u && METAL_MAX_MSAA_SAMPLES != 8u)
        {
            result.limits_valid = false;
            result.failure_reason = "METAL_MAX_MSAA_SAMPLES 必须为 1/2/4/8";
        }
        else if (METAL_MAX_THREADS_PER_THREADGROUP == 0u || METAL_MAX_THREADS_PER_THREADGROUP > 1024u)
        {
            result.limits_valid = false;
            result.failure_reason = "METAL_MAX_THREADS_PER_THREADGROUP 超出 [1, 1024]";
        }
    }

    result.valid = result.alignments_valid && result.limits_valid;
    return result;
}

// ════════════════════════════════════════════════════════════════════
// P4.1.1 — MetalDevice 创建与能力查询
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_device(metal_device** out_device)
{
    if (out_device == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::Device* mtl_device = MTL::CreateSystemDefaultDevice();

    if (mtl_device == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    metal_device* dev = new (std::nothrow) metal_device();
    if (dev == nullptr)
    {
        mtl_device->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    dev->base.type = METAL_HANDLE_TYPE_DEVICE;
    dev->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    dev->device = mtl_device;
    dev->error_buf[0] = '\0';
    dev->caps_populated = false;
    std::memset(&dev->caps, 0, sizeof(dev->caps));

    pool->release();
    *out_device = dev;
    return METAL_RESULT_OK;
}

metal_result metal_get_device_info(
    metal_device* device,
    metal_handle_info* out_info)
{
    if (device == nullptr || out_info == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    out_info->abi_version = METAL_BRIDGE_ABI_VERSION;
    out_info->reserved = 0;
    return METAL_RESULT_OK;
}

metal_result metal_get_device_caps(
    metal_device* device,
    metal_device_caps* out_caps)
{
    if (device == nullptr || out_caps == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // metal-cpp 属性访问器返回 autoreleased 对象，需要 AutoreleasePool
    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    populate_caps(device);
    std::memcpy(out_caps, &device->caps, sizeof(metal_device_caps));
    pool->release();

    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// P4.1.1 — MetalQueue 创建
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_queue(
    metal_device* device,
    metal_queue** out_queue)
{
    if (device == nullptr || out_queue == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)
    {
        set_device_error(device, "设备已被释放");
        return METAL_RESULT_RUNTIME_ERROR;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();
    MTL::CommandQueue* mtl_queue = device->device->newCommandQueue();

    if (mtl_queue == nullptr)
    {
        pool->release();
        set_device_error(device, "无法创建 MTLCommandQueue");
        return METAL_RESULT_RUNTIME_ERROR;
    }

    metal_queue* queue = new (std::nothrow) metal_queue();
    if (queue == nullptr)
    {
        mtl_queue->release();
        pool->release();
        set_device_error(device, "内存不足：无法分配 metal_queue");
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    queue->base.type = METAL_HANDLE_TYPE_QUEUE;
    queue->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    queue->queue = mtl_queue;

    pool->release();
    *out_queue = queue;
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// 编译器入口（stub，Phase 4.2 补全）
// ════════════════════════════════════════════════════════════════════

metal_result metal_acquire_shader_compiler(
    metal_device* device,
    metal_shader_compiler** out_compiler)
{
    (void)device;
    if (out_compiler == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;
    *out_compiler = nullptr;
    return METAL_RESULT_UNSUPPORTED;
}

metal_result metal_get_default_shader_compiler_config(
    metal_shader_compiler_config* out_config)
{
    if (out_config == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    out_config->abi_version = METAL_BRIDGE_ABI_VERSION;
    out_config->enabled_workarounds = METAL_WA_COMPILER_SINGLETON | METAL_WA_LANG_VERSION_3_2;
    out_config->disabled_workarounds = 0;
    out_config->metal_language_version = 0x00030000u;
    out_config->reserved = 0;
    return METAL_RESULT_OK;
}

metal_result metal_configure_shader_compiler(
    metal_shader_compiler* compiler,
    const metal_shader_compiler_config* config)
{
    (void)compiler;
    (void)config;
    return METAL_RESULT_UNSUPPORTED;
}

uint32_t metal_shader_compiler_get_workarounds(
    metal_shader_compiler* compiler)
{
    (void)compiler;
    return 0u;
}

// ════════════════════════════════════════════════════════════════════
// 公共函数：版本、释放、错误消息
// ════════════════════════════════════════════════════════════════════

uint32_t metal_bridge_abi_version(void)
{
    return METAL_BRIDGE_ABI_VERSION;
}

void metal_release(void* handle)
{
    if (handle == nullptr)
        return;

    // 通过公共头部 type tag 分发到正确的析构逻辑
    metal_handle_base* base = static_cast<metal_handle_base*>(handle);

    switch (base->type)
    {
    case METAL_HANDLE_TYPE_DEVICE:
    {
        metal_device* dev = static_cast<metal_device*>(handle);
        if (dev->device != nullptr)
        {
            dev->device->release();
            dev->device = nullptr;
        }
        delete dev;
        break;
    }
    case METAL_HANDLE_TYPE_QUEUE:
    {
        metal_queue* q = static_cast<metal_queue*>(handle);
        if (q->queue != nullptr)
        {
            q->queue->release();
            q->queue = nullptr;
        }
        delete q;
        break;
    }
    case METAL_HANDLE_TYPE_BUFFER:
    {
        metal_buffer* buf = static_cast<metal_buffer*>(handle);
        if (buf->buffer != nullptr)
        {
            buf->buffer->release();
            buf->buffer = nullptr;
        }
        delete buf;
        break;
    }
    case METAL_HANDLE_TYPE_TEXTURE:
    {
        metal_texture* tex = static_cast<metal_texture*>(handle);
        if (tex->texture != nullptr)
        {
            tex->texture->release();
            tex->texture = nullptr;
        }
        delete tex;
        break;
    }
    default:
        // 未知 type tag：不做任何操作
        // 这是一个防御性检查，防止未注册的 handle 类型被错误释放
        break;
    }
}

const char* metal_get_last_error_message(void)
{
    // TODO: P4.2 实现线程局部存储的错误消息
    return nullptr;
}
