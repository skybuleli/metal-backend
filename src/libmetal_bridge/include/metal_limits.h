// metal_limits.h — Metal 硬件限制常量与资源对齐策略
//
// 作用：集中固化 Metal 对 Apple GPU Family 7 (M1) 的硬件能力约束，
//       确保 C++ 实现与 C# P/Invoke 层使用同一套常量，避免因硬编码
//       漂移导致的资源分配越界或对齐错误。
//
// 涵盖：
//   - 缓冲区/纹理/采样器/计算/视口能力上限
//   - 各组件的对齐字节要求
//   - 存储模式选择策略
//   - 运行时校验辅助函数
//
// 参考来源：
//   - Apple Metal Feature Set Tables (macOS_GPUFamily2_v1)
//   - MTLDevice 运行时查询 API（device->maxBufferLength 等）
//   - M1 (Apple7) 实际硬件限制
//
// 本文件中的常量以 Apple7 (M1) 为基准。若后续支持更高 GPU Family，
// 应扩散为按 GPU Family 分组的宏或运行时查询。
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ════════════════════════════════════════════════════════════════════
// 1. 缓冲区能力上限
// ════════════════════════════════════════════════════════════════════

/// 每个着色器阶段最大缓冲区绑定数（vertex/fragment/compute 均为 31）
#define METAL_MAX_BUFFERS_PER_STAGE         31u

/// 最大顶点属性描述符数
#define METAL_MAX_VERTEX_ATTRIBUTES         31u

/// 最大顶点缓冲区绑定槽位数
#define METAL_MAX_VERTEX_BUFFER_BINDINGS    31u

/// 单个 MTLBuffer 最大长度（Private 模式可达 1GB+，此值取 Shared/Managed 上限）
/// 实际运行时需通过 device->maxBufferLength 精确查询。
#define METAL_MAX_BUFFER_LENGTH_DEFAULT     (256ull * 1024ull * 1024ull)  // 256 MB

/// 最小 uniform/constant 缓冲区大小（不得为 0）
#define METAL_MIN_CONSTANT_BUFFER_SIZE      16u

// ════════════════════════════════════════════════════════════════════
// 2. 纹理能力上限
// ════════════════════════════════════════════════════════════════════

/// 每个着色器阶段最大纹理绑定数
#define METAL_MAX_TEXTURES_PER_STAGE        128u

/// 最大渲染目标颜色附件数
#define METAL_MAX_COLOR_ATTACHMENTS         8u

/// 2D 纹理最大宽/高
#define METAL_MAX_2D_TEXTURE_DIM            16384u

/// Cube 纹理最大边长
#define METAL_MAX_CUBE_TEXTURE_DIM          16384u

/// 3D 纹理最大宽/高/深
#define METAL_MAX_3D_TEXTURE_DIM            2048u

/// 纹理数组最大层数
#define METAL_MAX_TEXTURE_ARRAY_LAYERS      2048u

/// 支持的最大 MSAA 采样数
#define METAL_MAX_MSAA_SAMPLES              8u

/// 支持的最小 MSAA 采样数
#define METAL_MIN_MSAA_SAMPLES              1u

// ════════════════════════════════════════════════════════════════════
// 3. 采样器能力上限
// ════════════════════════════════════════════════════════════════════

/// 每个着色器阶段最大独立采样器数
#define METAL_MAX_SAMPLERS_PER_STAGE        16u

// ════════════════════════════════════════════════════════════════════
// 4. Compute / Threadgroup 能力上限
// ════════════════════════════════════════════════════════════════════

/// 最大线程组线程数
#define METAL_MAX_THREADS_PER_THREADGROUP   1024u

/// 最大线程组共享内存（字节）
#define METAL_MAX_THREADGROUP_MEMORY_BYTES  32768u  // 32 KB

/// 最大线程组共享内存（KB）
#define METAL_MAX_THREADGROUP_MEMORY_KB     32u

/// 线程组网格各维最大尺寸
#define METAL_MAX_THREADGROUP_GRID_DIM      1024u

/// SIMD 组大小（Apple GPU 固定为 32）
#define METAL_SIMD_GROUP_SIZE               32u

/// 最大总线程数（dispatchThreadgroups 总线程数上限）
#define METAL_MAX_TOTAL_THREADS             2147483647u  // 2^31 - 1

// ════════════════════════════════════════════════════════════════════
// 5. 视口 / 裁剪能力上限
// ════════════════════════════════════════════════════════════════════

/// 最大视口数（macOS 所有 GPU Family 均支持 16）
#define METAL_MAX_VIEWPORTS                 16u

/// 最大深度裁剪平面数
#define METAL_MAX_DEPTH_CLIP_PLANES         16u

/// 渲染目标最大宽
#define METAL_MAX_RENDER_TARGET_WIDTH       16384u

/// 渲染目标最大高
#define METAL_MAX_RENDER_TARGET_HEIGHT      16384u

// ════════════════════════════════════════════════════════════════════
// 6. 对齐字节要求
// ════════════════════════════════════════════════════════════════════
//
// Metal 对资源偏移有严格的对齐约束。违反对齐会导致 GPU 崩溃
// 或未定义行为。以下常量为 Metal API 规定的最小对齐值。
//
// ════════════════════════════════════════════════════════════════════

/// 缓冲区偏移对齐（Metal 硬性要求：所有 setBuffer:offset: 偏移必须为此值的倍数）
#define METAL_BUFFER_OFFSET_ALIGNMENT       256u

/// 常量缓冲区对齐（uniform buffer 起始地址对齐）
#define METAL_CONSTANT_BUFFER_ALIGNMENT     256u

/// Argument Buffer 对齐
#define METAL_ARGUMENT_BUFFER_ALIGNMENT     256u

/// 线性纹理行对齐（bytesPerRow 对齐）
#define METAL_TEXTURE_ROW_ALIGNMENT         64u

/// 缓冲区偏移最小对齐（非 uniform 缓冲，部分设备允许 4 字节）
#define METAL_MIN_BUFFER_OFFSET_ALIGNMENT   4u

/// 纹理缓冲区对齐（MTLBuffer 作为纹理数据的对齐）
#define METAL_TEXTURE_BUFFER_ALIGNMENT      64u

// ════════════════════════════════════════════════════════════════════
// 7. 存储模式选择策略
// ════════════════════════════════════════════════════════════════════
//
// 策略说明：
//   - Apple Silicon (UMA)：Shared 与 Managed 效果相同，首选 Shared（零拷贝）
//   - 独立 GPU (AMD)     ：CPU 读写使用 Managed，GPU 专用使用 Private
//   - Memoryless          ：仅用于不需要 CPU 访问的 MSAA 中间缓冲
//
// 运行时通过 hasUnifiedMemory 自动选择。此策略用于 Buffer/Texture
// 默认分配，代码中应优先使用策略宏而非硬编码存储模式。
//
// ════════════════════════════════════════════════════════════════════

/// UMA 设备（Apple Silicon）的默认存储模式
#define METAL_UMA_DEFAULT_STORAGE           METAL_STORAGE_MODE_SHARED

/// 非 UMA 设备（独立 GPU）的 CPU 可见存储模式
#define METAL_DISCRETE_CPU_VISIBLE_STORAGE  METAL_STORAGE_MODE_MANAGED

/// 非 UMA 设备（独立 GPU）的 GPU 专用存储模式
#define METAL_DISCRETE_GPU_ONLY_STORAGE     METAL_STORAGE_MODE_PRIVATE

/// MSAA 中间缓冲存储模式（无需 CPU 访问）
#define METAL_MSAA_RESOLVE_STORAGE          METAL_STORAGE_MODE_MEMORYLESS

// ════════════════════════════════════════════════════════════════════
// 8. 对齐辅助宏
// ════════════════════════════════════════════════════════════════════

/// 将 value 向上对齐到 alignment 的倍数（alignment 必须为 2 的幂）
#define METAL_ALIGN_UP(value, alignment) \
    (((value) + ((alignment) - 1u)) & ~((alignment) - 1u))

/// 检查 value 是否满足 alignment 对齐
#define METAL_IS_ALIGNED(value, alignment) \
    (((value) & ((alignment) - 1u)) == 0u)

/// 将缓冲区偏移对齐到 256 字节
#define METAL_ALIGN_BUFFER_OFFSET(offset) \
    METAL_ALIGN_UP((offset), METAL_BUFFER_OFFSET_ALIGNMENT)

/// 将纹理行字节数对齐到 64 字节
#define METAL_ALIGN_TEXTURE_ROW(bytesPerRow) \
    METAL_ALIGN_UP((bytesPerRow), METAL_TEXTURE_ROW_ALIGNMENT)

/// 将常量缓冲区大小对齐到 256 字节
#define METAL_ALIGN_CONSTANT_BUFFER_SIZE(size) \
    METAL_ALIGN_UP((size), METAL_CONSTANT_BUFFER_ALIGNMENT)

// ════════════════════════════════════════════════════════════════════
// 9. 运行时校验
// ════════════════════════════════════════════════════════════════════
//
// 以下函数供 MetalDevice 初始化时调用，确保实际设备能力不低于
// 本文件定义的最低基线。
//
// ════════════════════════════════════════════════════════════════════

/// 对齐校验结果
typedef struct metal_limits_validation
{
    /// 所有对齐常量是否合理（2 的幂且 >= 最小要求）
    bool alignments_valid;
    /// 所有上限常量是否在合理范围内（>0）
    bool limits_valid;
    /// 总体验证是否通过
    bool valid;
    /// 若失败，描述第一个失败原因
    const char* failure_reason;
} metal_limits_validation;

/// 对 metal_limits.h 中的常量进行编译期+运行期合理性校验。
/// 应在 Metal 设备创建后尽早调用一次。
metal_limits_validation metal_validate_limits(void);

#ifdef __cplusplus
}
#endif
