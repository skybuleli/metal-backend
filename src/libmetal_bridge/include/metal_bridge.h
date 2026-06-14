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
    METAL_HANDLE_TYPE_DEPTH_STENCIL_STATE = 18,
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
typedef struct metal_depth_stencil_state metal_depth_stencil_state;
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

// ── 着色器磁盘缓存（P4.2.4）──

/// 清空磁盘着色器缓存（删除 ~/Library/Caches/SwitchMetal/ 下所有内容）
METAL_BRIDGE_EXPORT metal_result metal_shader_cache_clear(void);

/// 通过缓存键直接加载 metallib 数据（跳过编译管线）
/// @param cache_key 64 字符 SHA256 hex 缓存键
/// @return compile_result（metallib_data 通过 malloc 分配，用 metal_free_shader_data 释放）
METAL_BRIDGE_EXPORT metal_shader_compile_result metal_load_program_binary(
    const char* cache_key);

// ════════════════════════════════════════════════════════════════════
// 渲染管线状态（P4.3.1）
// ════════════════════════════════════════════════════════════════════

/// 顶点属性格式
/// 数值与 MTLVertexFormat 保持一致，便于 native 层直接转换。
typedef enum metal_vertex_format
{
    METAL_VERTEX_FORMAT_INVALID = 0,
    METAL_VERTEX_FORMAT_UCHAR2 = 1,
    METAL_VERTEX_FORMAT_UCHAR3 = 2,
    METAL_VERTEX_FORMAT_UCHAR4 = 3,
    METAL_VERTEX_FORMAT_CHAR2 = 4,
    METAL_VERTEX_FORMAT_CHAR3 = 5,
    METAL_VERTEX_FORMAT_CHAR4 = 6,
    METAL_VERTEX_FORMAT_UCHAR2_NORMALIZED = 7,
    METAL_VERTEX_FORMAT_UCHAR3_NORMALIZED = 8,
    METAL_VERTEX_FORMAT_UCHAR4_NORMALIZED = 9,
    METAL_VERTEX_FORMAT_CHAR2_NORMALIZED = 10,
    METAL_VERTEX_FORMAT_CHAR3_NORMALIZED = 11,
    METAL_VERTEX_FORMAT_CHAR4_NORMALIZED = 12,
    METAL_VERTEX_FORMAT_USHORT2 = 13,
    METAL_VERTEX_FORMAT_USHORT3 = 14,
    METAL_VERTEX_FORMAT_USHORT4 = 15,
    METAL_VERTEX_FORMAT_SHORT2 = 16,
    METAL_VERTEX_FORMAT_SHORT3 = 17,
    METAL_VERTEX_FORMAT_SHORT4 = 18,
    METAL_VERTEX_FORMAT_USHORT2_NORMALIZED = 19,
    METAL_VERTEX_FORMAT_USHORT3_NORMALIZED = 20,
    METAL_VERTEX_FORMAT_USHORT4_NORMALIZED = 21,
    METAL_VERTEX_FORMAT_SHORT2_NORMALIZED = 22,
    METAL_VERTEX_FORMAT_SHORT3_NORMALIZED = 23,
    METAL_VERTEX_FORMAT_SHORT4_NORMALIZED = 24,
    METAL_VERTEX_FORMAT_HALF2 = 25,
    METAL_VERTEX_FORMAT_HALF3 = 26,
    METAL_VERTEX_FORMAT_HALF4 = 27,
    METAL_VERTEX_FORMAT_FLOAT = 28,
    METAL_VERTEX_FORMAT_FLOAT2 = 29,
    METAL_VERTEX_FORMAT_FLOAT3 = 30,
    METAL_VERTEX_FORMAT_FLOAT4 = 31,
    METAL_VERTEX_FORMAT_INT = 32,
    METAL_VERTEX_FORMAT_INT2 = 33,
    METAL_VERTEX_FORMAT_INT3 = 34,
    METAL_VERTEX_FORMAT_INT4 = 35,
    METAL_VERTEX_FORMAT_UINT = 36,
    METAL_VERTEX_FORMAT_UINT2 = 37,
    METAL_VERTEX_FORMAT_UINT3 = 38,
    METAL_VERTEX_FORMAT_UINT4 = 39,
    METAL_VERTEX_FORMAT_INT1010102_NORMALIZED = 40,
    METAL_VERTEX_FORMAT_UINT1010102_NORMALIZED = 41,
    METAL_VERTEX_FORMAT_UCHAR4_NORMALIZED_BGRA = 42,
    METAL_VERTEX_FORMAT_UCHAR = 45,
    METAL_VERTEX_FORMAT_CHAR = 46,
    METAL_VERTEX_FORMAT_UCHAR_NORMALIZED = 47,
    METAL_VERTEX_FORMAT_CHAR_NORMALIZED = 48,
    METAL_VERTEX_FORMAT_USHORT = 49,
    METAL_VERTEX_FORMAT_SHORT = 50,
    METAL_VERTEX_FORMAT_USHORT_NORMALIZED = 51,
    METAL_VERTEX_FORMAT_SHORT_NORMALIZED = 52,
    METAL_VERTEX_FORMAT_HALF = 53,
    METAL_VERTEX_FORMAT_FLOAT_RG11B10 = 54,
    METAL_VERTEX_FORMAT_FLOAT_RGB9E5 = 55,
} metal_vertex_format;

/// 顶点步进函数
typedef enum metal_vertex_step_function
{
    METAL_VERTEX_STEP_FUNCTION_CONSTANT = 0,
    METAL_VERTEX_STEP_FUNCTION_PER_VERTEX = 1,
    METAL_VERTEX_STEP_FUNCTION_PER_INSTANCE = 2,
} metal_vertex_step_function;

/// 单个顶点属性描述符
typedef struct metal_vertex_attribute_descriptor
{
    uint32_t attribute_index;
    uint32_t buffer_index;
    metal_vertex_format format;
    uint32_t offset;
} metal_vertex_attribute_descriptor;

/// 单个顶点缓冲布局描述符
typedef struct metal_vertex_buffer_layout_descriptor
{
    uint32_t buffer_index;
    uint32_t stride;
    metal_vertex_step_function step_function;
    uint32_t step_rate;
} metal_vertex_buffer_layout_descriptor;

/// 混合因子（P4.3.9）
/// 值与 MTL::BlendFactor 对齐
/// 参考：Metal Shading Language Specification - Table 5.3
typedef enum metal_blend_factor
{
    METAL_BLEND_FACTOR_ZERO = 0,
    METAL_BLEND_FACTOR_ONE = 1,
    METAL_BLEND_FACTOR_SRC_COLOR = 2,
    METAL_BLEND_FACTOR_ONE_MINUS_SRC_COLOR = 3,
    METAL_BLEND_FACTOR_SRC_ALPHA = 4,
    METAL_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA = 5,
    METAL_BLEND_FACTOR_DST_ALPHA = 6,
    METAL_BLEND_FACTOR_ONE_MINUS_DST_ALPHA = 7,
    METAL_BLEND_FACTOR_DST_COLOR = 8,
    METAL_BLEND_FACTOR_ONE_MINUS_DST_COLOR = 9,
    METAL_BLEND_FACTOR_SRC_ALPHA_SATURATE = 10,
    METAL_BLEND_FACTOR_BLEND_COLOR = 11,
    METAL_BLEND_FACTOR_ONE_MINUS_BLEND_COLOR = 12,
    METAL_BLEND_FACTOR_BLEND_ALPHA = 13,
    METAL_BLEND_FACTOR_ONE_MINUS_BLEND_ALPHA = 14,
    METAL_BLEND_FACTOR_SRC1_COLOR = 15,
    METAL_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR = 16,
    METAL_BLEND_FACTOR_SRC1_ALPHA = 17,
    METAL_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA = 18,
} metal_blend_factor;

/// 混合操作（P4.3.9）
/// 值与 MTL::BlendOperation 对齐
typedef enum metal_blend_operation
{
    METAL_BLEND_OPERATION_ADD = 0,
    METAL_BLEND_OPERATION_SUBTRACT = 1,
    METAL_BLEND_OPERATION_REVERSE_SUBTRACT = 2,
    METAL_BLEND_OPERATION_MIN = 3,
    METAL_BLEND_OPERATION_MAX = 4,
} metal_blend_operation;

/// 颜色写入掩码（P4.3.9）
/// 值与 MTL::ColorWriteMask 对齐
typedef enum metal_color_write_mask
{
    METAL_COLOR_WRITE_MASK_NONE = 0,
    METAL_COLOR_WRITE_MASK_RED = 1u << 3,
    METAL_COLOR_WRITE_MASK_GREEN = 1u << 2,
    METAL_COLOR_WRITE_MASK_BLUE = 1u << 1,
    METAL_COLOR_WRITE_MASK_ALPHA = 1u << 0,
    METAL_COLOR_WRITE_MASK_ALL = (1u << 3) | (1u << 2) | (1u << 1) | (1u << 0),
} metal_color_write_mask;

/// 单个颜色附件的混合状态描述符（P4.3.9）
typedef struct metal_blend_attachment_descriptor
{
    uint8_t blending_enabled;              ///< 是否启用混合（0=禁用，1=启用）
    uint8_t reserved_pad[3];               ///< 对齐填充
    metal_blend_factor src_rgb_factor;     ///< RGB 源混合因子
    metal_blend_factor dst_rgb_factor;     ///< RGB 目标混合因子
    metal_blend_operation rgb_operation;   ///< RGB 混合操作
    metal_blend_factor src_alpha_factor;   ///< Alpha 源混合因子
    metal_blend_factor dst_alpha_factor;   ///< Alpha 目标混合因子
    metal_blend_operation alpha_operation; ///< Alpha 混合操作
    uint32_t write_mask;                   ///< 颜色写入掩码（metal_color_write_mask 按位或）
} metal_blend_attachment_descriptor;

/// 渲染管线描述符
/// 从 metallib 数据和基本像素格式创建 MTLRenderPipelineState
typedef struct metal_render_pipeline_descriptor
{
    uint32_t abi_version;

    /// 顶点着色器 metallib 数据（调用者管理生命周期）
    const void* vertex_metallib_data;
    uint64_t vertex_metallib_size;

    /// 片段着色器 metallib 数据（调用者管理生命周期）
    const void* fragment_metallib_data;
    uint64_t fragment_metallib_size;

    /// 入口函数名（默认 "main"）
    const char* vertex_function;
    const char* fragment_function;

    /// 颜色附件像素格式（默认 BGRA8_UNORM = 22）
    metal_pixel_format color_attachment_format;
    /// 深度模板附件格式（传 METAL_PIXEL_FORMAT_INVALID = 0 表示无深度模板）
    metal_pixel_format depth_stencil_format;

    /// 顶点属性数与缓冲布局数
    uint32_t vertex_attribute_count;
    uint32_t vertex_buffer_layout_count;

    /// 顶点布局描述符数组
    metal_vertex_attribute_descriptor vertex_attributes[METAL_MAX_VERTEX_ATTRIBUTES];
    metal_vertex_buffer_layout_descriptor vertex_buffer_layouts[METAL_MAX_VERTEX_BUFFER_BINDINGS];

    /// 混合附件描述符数组指针（P4.3.9）
    /// 传 nullptr 表示使用默认混合（禁用混合，全写入）
    const metal_blend_attachment_descriptor* blend_attachments;
    /// 混合附件数量（最多 8）
    uint32_t blend_attachment_count;
    uint32_t reserved;
} metal_render_pipeline_descriptor;

typedef enum metal_primitive_type
{
    METAL_PRIMITIVE_TYPE_POINT = 0,
    METAL_PRIMITIVE_TYPE_LINE = 1,
    METAL_PRIMITIVE_TYPE_LINE_STRIP = 2,
    METAL_PRIMITIVE_TYPE_TRIANGLE = 3,
    METAL_PRIMITIVE_TYPE_TRIANGLE_STRIP = 4,
} metal_primitive_type;

typedef enum metal_index_type
{
    METAL_INDEX_TYPE_UINT16 = 0,
    METAL_INDEX_TYPE_UINT32 = 1,
} metal_index_type;

/// 创建渲染管线状态
/// @param device     Metal 设备句柄
/// @param descriptor 渲染管线描述符
/// @param out_pipeline 输出：metal_render_pipeline 句柄（通过 metal_release 释放）
METAL_BRIDGE_EXPORT metal_result metal_create_render_pipeline(
    metal_device* device,
    const metal_render_pipeline_descriptor* descriptor,
    metal_render_pipeline** out_pipeline);

/// 创建命令缓冲区
METAL_BRIDGE_EXPORT metal_result metal_begin_command_buffer(
    metal_queue* queue,
    metal_command_buffer** out_command_buffer);

// ════════════════════════════════════════════════════════════════════
// 渲染目标附件描述符（P4.3.7）
// ════════════════════════════════════════════════════════════════════

/// 加载动作
typedef enum metal_load_action
{
    METAL_LOAD_ACTION_DONT_CARE = 0,
    METAL_LOAD_ACTION_LOAD = 1,
    METAL_LOAD_ACTION_CLEAR = 2,
} metal_load_action;

/// 存储动作
typedef enum metal_store_action
{
    METAL_STORE_ACTION_DONT_CARE = 0,
    METAL_STORE_ACTION_STORE = 1,
    METAL_STORE_ACTION_MULTISAMPLE_RESOLVE = 2,
} metal_store_action;

/// 清除颜色（与 MTL::ClearColor 匹配，使用 double 分量）
typedef struct metal_clear_color
{
    double red;
    double green;
    double blue;
    double alpha;
} metal_clear_color;

/// 清除深度/模板值
typedef struct metal_clear_depth_stencil
{
    double depth;
    uint32_t stencil;
    uint32_t reserved;
} metal_clear_depth_stencil;

/// 颜色附件描述符
/// 传递给 metal_begin_render_encoding_with_targets 定义每个颜色附件的纹理与行为
typedef struct metal_color_attachment_descriptor
{
    metal_texture* texture;           ///< 颜色附件纹理句柄
    uint32_t level;                   ///< mip 层级
    uint32_t slice;                   ///< 数组切片/cube面索引
    metal_load_action load_action;    ///< 加载动作（DontCare/Load/Clear）
    metal_store_action store_action;  ///< 存储动作（DontCare/Store）
    metal_clear_color clear_color;    ///< 清除颜色（仅 load_action==CLEAR 时使用）
} metal_color_attachment_descriptor;

/// 深度/模板附件描述符
typedef struct metal_depth_stencil_attachment_descriptor
{
    metal_texture* texture;                  ///< 深度/模板纹理句柄（nullptr 表示无）
    uint32_t level;                          ///< mip 层级
    uint32_t slice;                          ///< 数组切片
    metal_load_action depth_load_action;     ///< 深度加载动作
    metal_store_action depth_store_action;   ///< 深度存储动作
    metal_load_action stencil_load_action;   ///< 模板加载动作
    metal_store_action stencil_store_action; ///< 模板存储动作
    metal_clear_depth_stencil clear_value;   ///< 清除值
} metal_depth_stencil_attachment_descriptor;

/// 基于内部临时 1x1 颜色附件开始一次最小 render encoding。
/// 当前仅用于 P4.3.6 打通 Draw/DrawIndexed 链路；
/// 后续 P4.3.7 将由真实的 SetRenderTargets 提供 MTLRenderPassDescriptor。
METAL_BRIDGE_EXPORT metal_result metal_begin_render_encoding(
    metal_command_buffer* command_buffer,
    metal_render_pipeline* pipeline,
    metal_render_encoder** out_render_encoder);

/// 使用指定的渲染目标开始 render encoding（P4.3.7）
/// 替代 metal_begin_render_encoding 的临时 1x1 附件，创建真实的
/// MTLRenderPassDescriptor 并绑定颜色附件和可选的深度/模板附件。
/// @param command_buffer       命令缓冲区句柄
/// @param pipeline              渲染管线句柄
/// @param color_attachments     颜色附件描述符数组
/// @param color_attachment_count 颜色附件数（0 ~ METAL_MAX_COLOR_ATTACHMENTS）
/// @param depth_stencil         深度/模板附件描述符（传 nullptr 表示无）
/// @param out_render_encoder    输出：渲染编码器句柄
METAL_BRIDGE_EXPORT metal_result metal_begin_render_encoding_with_targets(
    metal_command_buffer* command_buffer,
    metal_render_pipeline* pipeline,
    const metal_color_attachment_descriptor* color_attachments,
    uint32_t color_attachment_count,
    const metal_depth_stencil_attachment_descriptor* depth_stencil,
    metal_render_encoder** out_render_encoder);

METAL_BRIDGE_EXPORT metal_result metal_render_encoder_set_vertex_buffer(
    metal_render_encoder* encoder,
    uint32_t index,
    metal_buffer* buffer,
    uint64_t offset);

METAL_BRIDGE_EXPORT metal_result metal_render_encoder_set_fragment_buffer(
    metal_render_encoder* encoder,
    uint32_t index,
    metal_buffer* buffer,
    uint64_t offset);

METAL_BRIDGE_EXPORT metal_result metal_render_encoder_set_fragment_texture(
    metal_render_encoder* encoder,
    uint32_t index,
    metal_texture* texture);

METAL_BRIDGE_EXPORT metal_result metal_render_encoder_set_fragment_sampler(
    metal_render_encoder* encoder,
    uint32_t index,
    metal_sampler* sampler);

METAL_BRIDGE_EXPORT metal_result metal_render_encoder_draw_primitives(
    metal_render_encoder* encoder,
    metal_primitive_type primitive_type,
    uint32_t vertex_start,
    uint32_t vertex_count,
    uint32_t instance_count,
    uint32_t base_instance);

METAL_BRIDGE_EXPORT metal_result metal_render_encoder_draw_indexed_primitives(
    metal_render_encoder* encoder,
    metal_primitive_type primitive_type,
    uint32_t index_count,
    metal_index_type index_type,
    metal_buffer* index_buffer,
    uint64_t index_buffer_offset,
    uint32_t instance_count,
    int32_t base_vertex,
    uint32_t base_instance);

METAL_BRIDGE_EXPORT metal_result metal_end_render_encoding(
    metal_render_encoder* encoder);

METAL_BRIDGE_EXPORT metal_result metal_commit_command_buffer(
    metal_command_buffer* command_buffer);

METAL_BRIDGE_EXPORT metal_result metal_wait_command_buffer(
    metal_command_buffer* command_buffer);

// ════════════════════════════════════════════════════════════════
// 深度/模板状态（P4.3.10）
// ════════════════════════════════════════════════════════════════

/// 比较函数复用上方 metal_compare_function

/// 模板操作（值与 MTL::StencilOperation 对齐）
typedef enum metal_stencil_operation
{
    METAL_STENCIL_OPERATION_KEEP = 0,
    METAL_STENCIL_OPERATION_ZERO = 1,
    METAL_STENCIL_OPERATION_REPLACE = 2,
    METAL_STENCIL_OPERATION_INCREMENT_CLAMP = 3,
    METAL_STENCIL_OPERATION_DECREMENT_CLAMP = 4,
    METAL_STENCIL_OPERATION_INVERT = 5,
    METAL_STENCIL_OPERATION_INCREMENT_WRAP = 6,
    METAL_STENCIL_OPERATION_DECREMENT_WRAP = 7,
} metal_stencil_operation;

/// 单面模板描述符
typedef struct metal_stencil_descriptor
{
    metal_compare_function compare_function;
    metal_stencil_operation stencil_failure;       ///< 模板测试失败时操作
    metal_stencil_operation depth_failure;          ///< 模板通过但深度失败时操作
    metal_stencil_operation depth_stencil_pass;     ///< 模板和深度均通过时操作
    uint32_t read_mask;                             ///< 读取掩码
    uint32_t write_mask;                            ///< 写入掩码
} metal_stencil_descriptor;

/// 深度/模板状态描述符
typedef struct metal_depth_stencil_descriptor
{
    metal_compare_function depth_compare_function;  ///< 深度比较函数
    uint8_t depth_write_enabled;                    ///< 是否启用深度写入
    uint8_t stencil_enabled;                        ///< 是否启用模板测试
    uint8_t reserved_pad[2];                        ///< 对齐填充
    metal_stencil_descriptor front_face;            ///< 正面模板状态
    metal_stencil_descriptor back_face;             ///< 背面模板状态
} metal_depth_stencil_descriptor;

/// 创建深度/模板状态对象
METAL_BRIDGE_EXPORT metal_result metal_create_depth_stencil_state(
    metal_device* device,
    const metal_depth_stencil_descriptor* descriptor,
    metal_depth_stencil_state** out_state);

/// 在渲染编码器上设置深度/模板状态
METAL_BRIDGE_EXPORT metal_result metal_render_encoder_set_depth_stencil_state(
    metal_render_encoder* encoder,
    metal_depth_stencil_state* state);

/// 设置模板引用值（对应 setStencilReferenceValue）
METAL_BRIDGE_EXPORT metal_result metal_render_encoder_set_stencil_reference_value(
    metal_render_encoder* encoder,
    uint32_t front_value,
    uint32_t back_value);

#ifdef __cplusplus
}
#endif
