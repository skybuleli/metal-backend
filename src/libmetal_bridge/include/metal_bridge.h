// metal_bridge.h — C ABI 头文件骨架
// Phase 3 先固化 ABI 边界：opaque handle、基础枚举、版本与错误码。
// 具体函数在 Phase 4 按模块逐步补齐，避免 C# 与 C++ 两侧接口反复返工。
#pragma once

#include <stddef.h>
#include <stdint.h>
#include "metal_limits.h"

#ifdef __cplusplus
extern "C" {
#endif

// ── 版本信息 ──
#define METAL_BRIDGE_ABI_VERSION 1u

// ── 导出宏 ──
#if defined(_WIN32)
#define METAL_BRIDGE_EXPORT __declspec(dllexport)
#else
#define METAL_BRIDGE_EXPORT __attribute__((visibility("default")))
#endif

// ── Opaque handle type tag：统一放在每个内部结构体的头部 ──
typedef enum metal_handle_type
{
    METAL_HANDLE_TYPE_DEVICE = 1,
    METAL_HANDLE_TYPE_QUEUE = 2,
    METAL_HANDLE_TYPE_BUFFER = 3,
    METAL_HANDLE_TYPE_TEXTURE = 4,
    METAL_HANDLE_TYPE_SAMPLER = 5,
    METAL_HANDLE_TYPE_LIBRARY = 6,
    METAL_HANDLE_TYPE_SHADER_COMPILER = 7,
    METAL_HANDLE_TYPE_RENDER_PIPELINE = 8,
    METAL_HANDLE_TYPE_COMPUTE_PIPELINE = 9,
    METAL_HANDLE_TYPE_COMMAND_BUFFER = 10,
    METAL_HANDLE_TYPE_RENDER_ENCODER = 11,
    METAL_HANDLE_TYPE_COMPUTE_ENCODER = 12,
    METAL_HANDLE_TYPE_BLIT_ENCODER = 13,
    METAL_HANDLE_TYPE_PRESENTER = 14,
    METAL_HANDLE_TYPE_FENCE = 15,
    METAL_HANDLE_TYPE_SHARED_EVENT = 16,
} metal_handle_type;

// ── 所有内部结构体的公共头部：type tag + ABI 版本校验 ──
typedef struct metal_handle_base
{
    metal_handle_type type;
    uint32_t abi_version;
} metal_handle_base;

// ── Opaque handle：C# 侧统一以 nint/IntPtr 持有 ──
typedef struct metal_device metal_device;
typedef struct metal_queue metal_queue;
typedef struct metal_buffer metal_buffer;
typedef struct metal_texture metal_texture;
typedef struct metal_sampler metal_sampler;
typedef struct metal_library metal_library;
typedef struct metal_shader_compiler metal_shader_compiler;
typedef struct metal_render_pipeline metal_render_pipeline;
typedef struct metal_compute_pipeline metal_compute_pipeline;
typedef struct metal_command_buffer metal_command_buffer;
typedef struct metal_render_encoder metal_render_encoder;
typedef struct metal_compute_encoder metal_compute_encoder;
typedef struct metal_blit_encoder metal_blit_encoder;
typedef struct metal_presenter metal_presenter;
typedef struct metal_fence metal_fence;
typedef struct metal_shared_event metal_shared_event;

// ── 通用返回码：不暴露 ObjC/metal-cpp 细节到 P/Invoke 边界 ──
typedef enum metal_result
{
    METAL_RESULT_OK = 0,
    METAL_RESULT_INVALID_ARGUMENT = 1,
    METAL_RESULT_UNSUPPORTED = 2,
    METAL_RESULT_OUT_OF_MEMORY = 3,
    METAL_RESULT_COMPILE_FAILED = 4,
    METAL_RESULT_RUNTIME_ERROR = 5,
} metal_result;

// ── 存储模式：与后续 MetalBuffer/MetalTexture 策略保持一致 ──
typedef enum metal_storage_mode
{
    METAL_STORAGE_MODE_SHARED = 0,
    METAL_STORAGE_MODE_MANAGED = 1,
    METAL_STORAGE_MODE_PRIVATE = 2,
    METAL_STORAGE_MODE_MEMORYLESS = 3,
} metal_storage_mode;

// ── Workaround 位掩码：先固定开关名字与位分配，避免后续日志/配置漂移 ──
typedef enum metal_workaround_flags
{
    METAL_WA_COMPILER_SINGLETON = 1u << 0,
    METAL_WA_LANG_VERSION_3_2 = 1u << 1,
    METAL_WA_DISCARD_GUARD = 1u << 2,
    METAL_WA_HELPER_INVOCATION = 1u << 3,
    METAL_WA_SAMPLE_MASK = 1u << 4,
    METAL_WA_TESS_TO_COMPUTE = 1u << 5,
    METAL_WA_TEXTURE_FORMAT = 1u << 6,
} metal_workaround_flags;

// ── 编译器配置：C# 侧按值传递，避免复杂 descriptor 提前冻结 ──
typedef struct metal_shader_compiler_config
{
    uint32_t abi_version;
    uint32_t enabled_workarounds;
    uint32_t disabled_workarounds;
    uint32_t metal_language_version;
    uint32_t reserved;
} metal_shader_compiler_config;

// ── 资源句柄元信息：供 C# 侧做安全校验与日志记录 ──
typedef struct metal_handle_info
{
    uint32_t abi_version;
    uint32_t reserved;
} metal_handle_info;

// ── 设备能力查询：在 C ABI 边界用纯 C 类型，不暴露 metal-cpp 枚举 ──
typedef struct metal_device_caps
{
    /// GPU 名称，例如 "Apple M1"
    char device_name[64];
    /// 是否使用统一内存架构（UMA）
    bool has_unified_memory;
    /// 设备注册 ID（registryID）
    uint64_t registry_id;
    /// 最大缓冲区长度（字节）
    uint64_t max_buffer_length;
    /// 各维度最大线程组线程数
    uint32_t max_threads_per_threadgroup_x;
    uint32_t max_threads_per_threadgroup_y;
    uint32_t max_threads_per_threadgroup_z;
    /// 最大线程组共享内存大小（字节）
    uint32_t max_threadgroup_memory;
    /// 最大参数缓冲采样器数
    uint32_t max_argument_buffer_sampler_count;
    /// 是否支持 Apple GPU Family 7（M1）
    bool supports_apple7;
    /// 是否支持 Mac GPU Family 1
    bool supports_mac1;
    /// 最大颜色附件数
    uint32_t max_color_attachments;
    /// 最大视口数
    uint32_t max_viewports;
    /// 保留字段（对齐到 128 字节）
    uint32_t reserved[8];
} metal_device_caps;

// ── 生命周期函数：全部模块共用 release，避免每类对象单独暴露销毁入口 ──
METAL_BRIDGE_EXPORT uint32_t metal_bridge_abi_version(void);
METAL_BRIDGE_EXPORT void metal_release(void* handle);

// ── 错误访问：先收口统一错误读取方式，具体线程模型后续再细化 ──
METAL_BRIDGE_EXPORT const char* metal_get_last_error_message(void);

// ── 设备入口 ──
METAL_BRIDGE_EXPORT metal_result metal_create_device(metal_device** out_device);
METAL_BRIDGE_EXPORT metal_result metal_get_device_info(
    metal_device* device,
    metal_handle_info* out_info);
METAL_BRIDGE_EXPORT metal_result metal_get_device_caps(
    metal_device* device,
    metal_device_caps* out_caps);

// ── 队列入口：Phase 4.4 将在此基础上补 command buffer / sync / present ──
METAL_BRIDGE_EXPORT metal_result metal_create_queue(
    metal_device* device,
    metal_queue** out_queue);

// ── 编译器入口：Phase 3.1b 将补单例与 workaround 设计，Phase 4.2 再补具体编译 API ──
METAL_BRIDGE_EXPORT metal_result metal_acquire_shader_compiler(
    metal_device* device,
    metal_shader_compiler** out_compiler);
METAL_BRIDGE_EXPORT metal_result metal_get_default_shader_compiler_config(
    metal_shader_compiler_config* out_config);
METAL_BRIDGE_EXPORT metal_result metal_configure_shader_compiler(
    metal_shader_compiler* compiler,
    const metal_shader_compiler_config* config);
METAL_BRIDGE_EXPORT uint32_t metal_shader_compiler_get_workarounds(
    metal_shader_compiler* compiler);

// TODO: Phase 4
// - metal_create_buffer / metal_map_buffer / metal_unmap_buffer
// - metal_create_texture / metal_upload_texture / metal_readback_texture
// - metal_create_sampler
// - metal_compile_program / metal_create_render_pipeline / metal_create_compute_pipeline
// - metal_begin_command_buffer / metal_commit_command_buffer / metal_wait_command_buffer
// - metal_presenter_* / metal_sync_* / metal_encoder_* 系列接口

#ifdef __cplusplus
}
#endif
