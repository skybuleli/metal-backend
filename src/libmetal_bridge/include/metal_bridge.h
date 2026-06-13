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

// ── 所有 Handle 类型内部结构体的公共前缀：确保 metal_handle_base 在偏移 0 ──
#define METAL_HANDLE_HEADER \
    metal_handle_base base;

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
    METAL_HANDLE_TYPE_HEAP = 17,
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
typedef struct metal_heap metal_heap;

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

// ── 缓冲区信息 ──
typedef struct metal_buffer_info
{
    /// 缓冲区大小（字节）
    uint64_t size;
    /// 存储模式
    metal_storage_mode storage_mode;
    /// 对齐填充到 16 字节
    uint32_t reserved;
} metal_buffer_info;

// ── 缓冲区入口：P4.1.2 MetalBuffer 完整实现 ──
METAL_BRIDGE_EXPORT metal_result metal_create_buffer(
    metal_device* device,
    uint64_t size,
    metal_storage_mode mode,
    metal_buffer** out_buffer);

METAL_BRIDGE_EXPORT metal_result metal_create_buffer_with_bytes(
    metal_device* device,
    const void* data,
    uint64_t size,
    metal_storage_mode mode,
    metal_buffer** out_buffer);

METAL_BRIDGE_EXPORT metal_result metal_buffer_get_info(
    metal_buffer* buffer,
    metal_buffer_info* out_info);

METAL_BRIDGE_EXPORT metal_result metal_map_buffer(
    metal_buffer* buffer,
    void** out_ptr);

METAL_BRIDGE_EXPORT metal_result metal_unmap_buffer(
    metal_buffer* buffer);

METAL_BRIDGE_EXPORT metal_result metal_flush_buffer(
    metal_buffer* buffer,
    uint64_t offset,
    uint64_t size);

// ── 缓冲区零拷贝包装（P4.1.2 补充）：包装已有的 CPU 内存指针 ──
METAL_BRIDGE_EXPORT metal_result metal_create_buffer_from_pointer(
    metal_device* device,
    void* ptr,
    uint64_t size,
    metal_storage_mode mode,
    metal_buffer** out_buffer);

// ── 缓冲区 CPU 地址直接访问（UMA 优化，无需 map/unmap 配对）──
METAL_BRIDGE_EXPORT metal_result metal_buffer_get_cpu_address(
    metal_buffer* buffer,
    void** out_ptr);

// ════════════════════════════════════════════════════════════════════
// 纹理类型与像素格式枚举（P4.1.3）
// ════════════════════════════════════════════════════════════════════

typedef enum metal_texture_usage
{
    METAL_TEXTURE_USAGE_UNKNOWN = 0,
    METAL_TEXTURE_USAGE_SHADER_READ = 1u << 0,
    METAL_TEXTURE_USAGE_SHADER_WRITE = 1u << 1,
    METAL_TEXTURE_USAGE_RENDER_TARGET = 1u << 2,
    METAL_TEXTURE_USAGE_PIXEL_FORMAT_VIEW = 1u << 3,
} metal_texture_usage;

typedef enum metal_texture_type
{
    METAL_TEXTURE_TYPE_2D = 0,
    METAL_TEXTURE_TYPE_2D_ARRAY = 1,
    METAL_TEXTURE_TYPE_CUBE = 2,
    METAL_TEXTURE_TYPE_3D = 3,
    METAL_TEXTURE_TYPE_2D_MULTISAMPLE = 4,
} metal_texture_type;

typedef enum metal_pixel_format
{
    METAL_PIXEL_FORMAT_INVALID = 0,
    // 8-bit
    METAL_PIXEL_FORMAT_R8_UNORM = 1,
    METAL_PIXEL_FORMAT_R8_SNORM = 2,
    METAL_PIXEL_FORMAT_R8_UINT = 3,
    METAL_PIXEL_FORMAT_R8_SINT = 4,
    // 16-bit
    METAL_PIXEL_FORMAT_R16_FLOAT = 5,
    METAL_PIXEL_FORMAT_R16_UNORM = 6,
    METAL_PIXEL_FORMAT_R16_UINT = 7,
    METAL_PIXEL_FORMAT_R16_SINT = 8,
    METAL_PIXEL_FORMAT_RG8_UNORM = 9,
    // 32-bit
    METAL_PIXEL_FORMAT_R32_FLOAT = 10,
    METAL_PIXEL_FORMAT_R32_UINT = 11,
    METAL_PIXEL_FORMAT_R32_SINT = 12,
    METAL_PIXEL_FORMAT_RG16_FLOAT = 13,
    METAL_PIXEL_FORMAT_RG16_UNORM = 14,
    METAL_PIXEL_FORMAT_RG16_UINT = 15,
    METAL_PIXEL_FORMAT_RG16_SINT = 16,
    METAL_PIXEL_FORMAT_RGBA8_UNORM = 17,
    METAL_PIXEL_FORMAT_RGBA8_SNORM = 18,
    METAL_PIXEL_FORMAT_RGBA8_UINT = 19,
    METAL_PIXEL_FORMAT_RGBA8_SINT = 20,
    METAL_PIXEL_FORMAT_RGBA8_SRGB = 21,
    METAL_PIXEL_FORMAT_BGRA8_UNORM = 22,
    METAL_PIXEL_FORMAT_BGRA8_SRGB = 23,
    // 64-bit
    METAL_PIXEL_FORMAT_RG32_FLOAT = 24,
    METAL_PIXEL_FORMAT_RG32_UINT = 25,
    METAL_PIXEL_FORMAT_RG32_SINT = 26,
    METAL_PIXEL_FORMAT_RGBA16_FLOAT = 27,
    METAL_PIXEL_FORMAT_RGBA16_UNORM = 28,
    METAL_PIXEL_FORMAT_RGBA16_UINT = 29,
    METAL_PIXEL_FORMAT_RGBA16_SINT = 30,
    // 128-bit
    METAL_PIXEL_FORMAT_RGBA32_FLOAT = 31,
    METAL_PIXEL_FORMAT_RGBA32_UINT = 32,
    METAL_PIXEL_FORMAT_RGBA32_SINT = 33,
    // Packed
    METAL_PIXEL_FORMAT_R10G10B10A2_UNORM = 34,
    METAL_PIXEL_FORMAT_R11G11B10_FLOAT = 35,
    METAL_PIXEL_FORMAT_RGB9E5_FLOAT = 36,
    // Depth / Stencil
    METAL_PIXEL_FORMAT_D16_UNORM = 37,
    METAL_PIXEL_FORMAT_D32_FLOAT = 38,
    METAL_PIXEL_FORMAT_D24_UNORM_S8_UINT = 39,
    METAL_PIXEL_FORMAT_D32_FLOAT_S8_UINT = 40,
    // Compressed (BC)
    METAL_PIXEL_FORMAT_BC1_RGBA = 41,
    METAL_PIXEL_FORMAT_BC2_RGBA = 42,
    METAL_PIXEL_FORMAT_BC3_RGBA = 43,
    METAL_PIXEL_FORMAT_BC4_R = 44,
    METAL_PIXEL_FORMAT_BC5_RG = 45,
    METAL_PIXEL_FORMAT_BC6H_RGB = 46,
    METAL_PIXEL_FORMAT_BC7_RGBA = 47,
    // Compressed (ASTC)
    METAL_PIXEL_FORMAT_ASTC_4x4_LDR = 48,
    METAL_PIXEL_FORMAT_ASTC_6x6_LDR = 49,
    METAL_PIXEL_FORMAT_ASTC_8x8_LDR = 50,
    METAL_PIXEL_FORMAT_ASTC_12x12_LDR = 51,
    // Compressed (ETC2 — 在 Metal 侧展开到 RGBA8)
    METAL_PIXEL_FORMAT_ETC2_RGB = 52,
    METAL_PIXEL_FORMAT_ETC2_RGBA = 53,
} metal_pixel_format;

/// 像素格式元信息
#define METAL_PIXEL_FORMAT_NAME_MAX 32
typedef struct metal_pixel_format_info
{
    char name[METAL_PIXEL_FORMAT_NAME_MAX];
    uint32_t bytes_per_pixel;
    uint32_t block_width;
    uint32_t block_height;
    bool is_depth;
    bool is_compressed;
    bool is_srgb;
    uint32_t reserved;
} metal_pixel_format_info;

/// 纹理元信息
typedef struct metal_texture_info
{
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    uint32_t levels;
    uint32_t samples;
    metal_texture_type type;
    metal_pixel_format pixel_format;
    metal_storage_mode storage_mode;
    uint32_t reserved;
} metal_texture_info;

// ── 纹理入口：P4.1.3 MetalTexture 完整实现 ──
METAL_BRIDGE_EXPORT metal_result metal_create_texture(
    metal_device* device,
    metal_pixel_format format,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    uint32_t levels,
    uint32_t samples,
    metal_texture_type type,
    uint32_t usage_flags,
    metal_storage_mode storage_mode,
    metal_texture** out_texture);

METAL_BRIDGE_EXPORT metal_result metal_texture_get_info(
    metal_texture* texture,
    metal_texture_info* out_info);

METAL_BRIDGE_EXPORT metal_result metal_texture_upload(
    metal_texture* texture,
    metal_buffer* buffer,
    uint64_t buffer_offset,
    uint32_t layer,
    uint32_t level,
    uint32_t region_x,
    uint32_t region_y,
    uint32_t region_z,
    uint32_t region_width,
    uint32_t region_height,
    uint32_t bytes_per_row);

METAL_BRIDGE_EXPORT metal_result metal_texture_readback(
    metal_texture* texture,
    metal_buffer* buffer,
    uint64_t buffer_offset,
    uint32_t layer,
    uint32_t level,
    uint32_t bytes_per_row);

// ── 纹理视图：在现有纹理上创建共享底层存储的视图 ——
METAL_BRIDGE_EXPORT metal_result metal_create_texture_view(
    metal_texture* parent_texture,
    metal_pixel_format format,
    metal_texture_type type,
    uint32_t first_layer,
    uint32_t num_layers,
    uint32_t first_level,
    uint32_t num_levels,
    metal_texture** out_texture);

// ── 像素格式查询：供 C# 侧缓存使用 ———
METAL_BRIDGE_EXPORT metal_pixel_format_info metal_pixel_format_get_info(
    metal_pixel_format format);

// ════════════════════════════════════════════════════════════════════
// 采样器枚举（P4.1.4）
// ════════════════════════════════════════════════════════════════════

/// 最小/最大过滤器（线性/最近）
typedef enum metal_sampler_min_mag_filter
{
    METAL_SAMPLER_FILTER_NEAREST = 0,
    METAL_SAMPLER_FILTER_LINEAR = 1,
} metal_sampler_min_mag_filter;

/// mipmap 过滤器
typedef enum metal_sampler_mip_filter
{
    METAL_SAMPLER_MIP_FILTER_NOT_MIPMAPPED = 0,
    METAL_SAMPLER_MIP_FILTER_NEAREST = 1,
    METAL_SAMPLER_MIP_FILTER_LINEAR = 2,
} metal_sampler_mip_filter;

/// 寻址模式
typedef enum metal_sampler_address_mode
{
    METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE = 0,
    METAL_SAMPLER_ADDRESS_REPEAT = 1,
    METAL_SAMPLER_ADDRESS_MIRRORED_REPEAT = 2,
    METAL_SAMPLER_ADDRESS_CLAMP_TO_ZERO = 3,
    METAL_SAMPLER_ADDRESS_CLAMP_TO_BORDER_COLOR = 4,
    METAL_SAMPLER_ADDRESS_MIRROR_CLAMP_TO_EDGE = 5,
} metal_sampler_address_mode;

/// 比较函数
typedef enum metal_compare_function
{
    METAL_COMPARE_NEVER = 0,
    METAL_COMPARE_LESS = 1,
    METAL_COMPARE_EQUAL = 2,
    METAL_COMPARE_LESS_EQUAL = 3,
    METAL_COMPARE_GREATER = 4,
    METAL_COMPARE_NOT_EQUAL = 5,
    METAL_COMPARE_GREATER_EQUAL = 6,
    METAL_COMPARE_ALWAYS = 7,
} metal_compare_function;

/// 采样器描述符
typedef struct metal_sampler_descriptor
{
    metal_sampler_min_mag_filter mag_filter;
    metal_sampler_min_mag_filter min_filter;
    metal_sampler_mip_filter mip_filter;
    metal_sampler_address_mode address_s;
    metal_sampler_address_mode address_t;
    metal_sampler_address_mode address_r;
    metal_compare_function compare_function;
    float max_anisotropy;
    float lod_min_clamp;
    float lod_max_clamp;
    bool normalized_coordinates;
    uint32_t reserved[3];
} metal_sampler_descriptor;

// ── 采样器入口：P4.1.4 MetalSampler 完整实现 ──
METAL_BRIDGE_EXPORT metal_result metal_create_sampler(
    metal_device* device,
    const metal_sampler_descriptor* descriptor,
    metal_sampler** out_sampler);

// ── 堆入口：P4.1.5 稀疏缓冲区 MTLHeap + MTLBuffer ──

/// 创建 MTLHeap（用于稀疏资源）
/// @param device   Metal 设备句柄
/// @param size     堆总大小（字节）
/// @param mode     存储模式（通常为 METAL_STORAGE_MODE_PRIVATE）
/// @param out_heap 输出：metal_heap 句柄
METAL_BRIDGE_EXPORT metal_result metal_create_heap(
    metal_device* device,
    uint64_t size,
    metal_storage_mode mode,
    metal_heap** out_heap);

/// 从 MTLHeap 分配新的 MTLBuffer（指定偏移和大小）
/// @param heap      堆句柄
/// @param offset    在堆中的偏移（字节）
/// @param size      缓冲区大小（字节）
/// @param out_buffer 输出：metal_buffer 句柄（其生命周期绑定到 heap）
METAL_BRIDGE_EXPORT metal_result metal_heap_create_buffer(
    metal_heap* heap,
    uint64_t offset,
    uint64_t size,
    metal_buffer** out_buffer);

// ════════════════════════════════════════════════════════════════════
// 着色器编译（P4.2.1）
// ════════════════════════════════════════════════════════════════════

/// 编译结果 — 通过 metal_compile_shader 返回
typedef struct metal_shader_compile_result
{
    metal_result result;
    char error_message[512];
    uint32_t _pad0;             ///< 显式填充，保证 void* 8 字节对齐
    void* metallib_data;       ///< malloc 分配，调用者须通过 metal_free_shader_data 释放
    uint64_t metallib_size;
} metal_shader_compile_result;

// 编译期验证：C 侧内存布局与 C# 侧 Marshal 一致
#ifdef __cplusplus
static_assert(sizeof(metal_shader_compile_result) == 536,
              "metal_shader_compile_result 大小异常，需检查 C# MetalShaderCompileResult 对齐");
static_assert(offsetof(metal_shader_compile_result, metallib_data) == 520,
              "metallib_data 偏移异常，C# Marshal 会读错");
#endif

/// 编译单个着色器：Slang 原生语法 → DXIL → MSC → metallib
/// @param compiler   着色器编译器句柄（由 metal_acquire_shader_compiler 获取）
/// @param source_code Slang 原生语法源码（以 null 结尾）
/// @param stage      "vertex" / "fragment" / "compute"
/// @param entry_point 入口函数名（通常 "main"）
/// @param profile    "sm_6_0" / "ps_6_0" / "cs_6_0"
METAL_BRIDGE_EXPORT metal_shader_compile_result metal_compile_shader(
    metal_shader_compiler* compiler,
    const char* source_code,
    const char* stage,
    const char* entry_point,
    const char* profile);

/// 释放 metal_compile_shader 返回的 metallib 数据
METAL_BRIDGE_EXPORT void metal_free_shader_data(void* data);

// TODO: Phase 4.3+
// - metal_begin_command_buffer / metal_commit_command_buffer / metal_wait_command_buffer
// - metal_presenter_* / metal_sync_* / metal_encoder_* 系列接口

#ifdef __cplusplus
}
#endif
