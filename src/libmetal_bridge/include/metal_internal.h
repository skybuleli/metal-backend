// metal_internal.h — libmetal_bridge 内部结构体定义
//
// 本文件定义所有 opaque handle 类型的实际内部结构体布局。
// 这些结构体在 metal_bridge.h 中只前向声明为 typedef，完整定义在此处。
//
// 使用约定：
//   - 所有 .cpp 文件包含此文件而非自己定义结构体
//   - 结构体定义遵循 METAL_HANDLE_HEADER → 实际数据成员的顺序
//   - 此文件不暴露给 C# P/Invoke 层
#pragma once

#include "metal_bridge.h"
#include "metal_limits.h"

#include <Metal/Metal.hpp>
#include <Foundation/Foundation.hpp>

#include <cstring>
#include <cstdio>
#include <cstdarg>
#include <new>

// ════════════════════════════════════════════════════════════════════
// 内部辅助常量
// ════════════════════════════════════════════════════════════════════

static constexpr size_t kErrorBufSize = 256;

// ════════════════════════════════════════════════════════════════════
// Handle 结构体定义
// ════════════════════════════════════════════════════════════════════

/// metal_device 内部实现
struct metal_device
{
    METAL_HANDLE_HEADER
    MTL::Device* device;              // Metal 设备对象
    char error_buf[kErrorBufSize];    // 最后错误消息
    metal_device_caps caps;           // 缓存的能力查询结果
    bool caps_populated;              // caps 是否已填充
};

static_assert(offsetof(struct metal_device, device) >= sizeof(metal_handle_base),
    "metal_device.base 必须在最前面");

/// metal_queue 内部实现
struct metal_queue
{
    METAL_HANDLE_HEADER
    MTL::CommandQueue* queue;         // Metal 命令队列
};

static_assert(offsetof(struct metal_queue, queue) >= sizeof(metal_handle_base),
    "metal_queue.base 必须在最前面");

/// metal_buffer 内部实现
struct metal_buffer
{
    METAL_HANDLE_HEADER
    MTL::Buffer* buffer;              // 真实的 Metal 缓冲区
    size_t        size;               // 分配大小（字节）
    metal_storage_mode mode;          // 存储模式
};

static_assert(offsetof(struct metal_buffer, buffer) >= sizeof(metal_handle_base),
    "metal_buffer.base 必须在最前面");

/// metal_texture 内部实现
struct metal_texture
{
    METAL_HANDLE_HEADER
    MTL::Texture* texture;            // 真实的 Metal 纹理
    uint32_t      width;              // 纹理宽度
    uint32_t      height;             // 纹理高度
    uint32_t      depth;              // 纹理深度
    uint32_t      levels;             // mip 级数
    uint32_t      samples;            // MSAA 采样数
    metal_texture_type type;          // 纹理类型
    metal_pixel_format pixel_format;  // 像素格式
    metal_storage_mode storage_mode;  // 存储模式
};

static_assert(offsetof(struct metal_texture, texture) >= sizeof(metal_handle_base),
    "metal_texture.base 必须在最前面");

/// metal_sampler 内部实现
struct metal_sampler
{
    METAL_HANDLE_HEADER
    MTL::SamplerState* sampler_state;  // Metal 采样器状态对象
};

static_assert(offsetof(struct metal_sampler, sampler_state) >= sizeof(metal_handle_base),
    "metal_sampler.base 必须在最前面");

/// metal_heap 内部实现
struct metal_heap
{
    METAL_HANDLE_HEADER
    MTL::Heap* heap;                  // Metal 堆对象
    size_t     size;                  // 堆总大小（字节）
};

static_assert(offsetof(struct metal_heap, heap) >= sizeof(metal_handle_base),
    "metal_heap.base 必须在最前面");

/// metal_shader_compiler 内部实现
/// 注意：使用 calloc 分配，metal_release 中通过专用函数释放
struct metal_shader_compiler
{
    METAL_HANDLE_HEADER
    metal_device* device;             // 关联的 Metal 设备句柄
};

static_assert(offsetof(struct metal_shader_compiler, device) >= sizeof(metal_handle_base),
    "metal_shader_compiler.base 必须在最前面");

// ── 编译器内部释放函数：由 metal_release 调用，处理 Slang 会话清理 ──
void metal_shader_compiler_release(metal_shader_compiler* compiler);

/// metal_render_pipeline 内部实现（P4.3.1）
struct metal_render_pipeline
{
    METAL_HANDLE_HEADER
    MTL::RenderPipelineState* pipeline_state;  // Metal 渲染管线状态
};

static_assert(offsetof(struct metal_render_pipeline, pipeline_state) >= sizeof(metal_handle_base),
    "metal_render_pipeline.base 必须在最前面");

