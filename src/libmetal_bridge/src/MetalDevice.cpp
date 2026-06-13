// MetalDevice.cpp — Metal 设备管理：创建、能力查询、硬件限制校验

#include "metal_bridge.h"
#include "metal_limits.h"

#include <stddef.h>
#include <stdint.h>

// ════════════════════════════════════════════════════════════════════
// P4.1.0 — 硬件限制常量运行时校验
// ════════════════════════════════════════════════════════════════════

metal_limits_validation metal_validate_limits(void)
{
    metal_limits_validation result;
    result.valid = true;
    result.alignments_valid = true;
    result.limits_valid = true;
    result.failure_reason = NULL;

    // ── 校验对齐常量是否为 2 的幂且 >= 最小要求 ──

    // 缓冲区偏移对齐必须为 2 的幂
    if ((METAL_BUFFER_OFFSET_ALIGNMENT & (METAL_BUFFER_OFFSET_ALIGNMENT - 1u)) != 0u)
    {
        result.alignments_valid = false;
        result.failure_reason = "METAL_BUFFER_OFFSET_ALIGNMENT 不是 2 的幂";
    }

    if ((METAL_CONSTANT_BUFFER_ALIGNMENT & (METAL_CONSTANT_BUFFER_ALIGNMENT - 1u)) != 0u)
    {
        result.alignments_valid = false;
        result.failure_reason = "METAL_CONSTANT_BUFFER_ALIGNMENT 不是 2 的幂";
    }

    if ((METAL_TEXTURE_ROW_ALIGNMENT & (METAL_TEXTURE_ROW_ALIGNMENT - 1u)) != 0u)
    {
        result.alignments_valid = false;
        result.failure_reason = "METAL_TEXTURE_ROW_ALIGNMENT 不是 2 的幂";
    }

    // 对齐值不得低于 Metal 硬性要求
    if (METAL_BUFFER_OFFSET_ALIGNMENT < 4u)
    {
        result.alignments_valid = false;
        result.failure_reason = "METAL_BUFFER_OFFSET_ALIGNMENT 小于 4";
    }

    // ── 校验上限常量是否在合理范围内 ──

    if (METAL_MAX_BUFFERS_PER_STAGE == 0u || METAL_MAX_BUFFERS_PER_STAGE > 31u)
    {
        result.limits_valid = false;
        result.failure_reason = "METAL_MAX_BUFFERS_PER_STAGE 超出有效范围 [1, 31]";
    }

    if (METAL_MAX_TEXTURES_PER_STAGE == 0u || METAL_MAX_TEXTURES_PER_STAGE > 128u)
    {
        result.limits_valid = false;
        result.failure_reason = "METAL_MAX_TEXTURES_PER_STAGE 超出有效范围 [1, 128]";
    }

    if (METAL_MAX_SAMPLERS_PER_STAGE == 0u || METAL_MAX_SAMPLERS_PER_STAGE > 16u)
    {
        result.limits_valid = false;
        result.failure_reason = "METAL_MAX_SAMPLERS_PER_STAGE 超出有效范围 [1, 16]";
    }

    if (METAL_MAX_COLOR_ATTACHMENTS == 0u || METAL_MAX_COLOR_ATTACHMENTS > 8u)
    {
        result.limits_valid = false;
        result.failure_reason = "METAL_MAX_COLOR_ATTACHMENTS 超出有效范围 [1, 8]";
    }

    if (METAL_MAX_MSAA_SAMPLES != 1u && METAL_MAX_MSAA_SAMPLES != 2u &&
        METAL_MAX_MSAA_SAMPLES != 4u && METAL_MAX_MSAA_SAMPLES != 8u)
    {
        result.limits_valid = false;
        result.failure_reason = "METAL_MAX_MSAA_SAMPLES 必须为 1/2/4/8";
    }

    if (METAL_MAX_THREADS_PER_THREADGROUP == 0u || METAL_MAX_THREADS_PER_THREADGROUP > 1024u)
    {
        result.limits_valid = false;
        result.failure_reason = "METAL_MAX_THREADS_PER_THREADGROUP 超出有效范围 [1, 1024]";
    }

    // ── 汇总结果 ──
    result.valid = result.alignments_valid && result.limits_valid;
    return result;
}

// ════════════════════════════════════════════════════════════════════
// metal_bridge.h 声明的外部接口（P4.1.1 补全实现）
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_device(metal_device** out_device)
{
    if (out_device == NULL)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    // P4.1.1：此处将调用 MTL::CreateSystemDefaultDevice()
    *out_device = NULL;
    return METAL_RESULT_UNSUPPORTED;
}

metal_result metal_get_device_info(
    metal_device* device,
    metal_handle_info* out_info)
{
    if (device == NULL || out_info == NULL)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    // P4.1.1：此处将填充真实设备信息
    out_info->abi_version = METAL_BRIDGE_ABI_VERSION;
    out_info->reserved = 0;
    return METAL_RESULT_OK;
}

metal_result metal_create_queue(
    metal_device* device,
    metal_queue** out_queue)
{
    if (device == NULL || out_queue == NULL)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    // P4.1.1：此处将创建 MTLCommandQueue
    *out_queue = NULL;
    return METAL_RESULT_UNSUPPORTED;
}

metal_result metal_acquire_shader_compiler(
    metal_device* device,
    metal_shader_compiler** out_compiler)
{
    if (device == NULL || out_compiler == NULL)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    // P4.2.x：此处将复用或创建编译器实例
    *out_compiler = NULL;
    return METAL_RESULT_UNSUPPORTED;
}

metal_result metal_get_default_shader_compiler_config(
    metal_shader_compiler_config* out_config)
{
    if (out_config == NULL)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    out_config->abi_version = METAL_BRIDGE_ABI_VERSION;
    out_config->enabled_workarounds = METAL_WA_COMPILER_SINGLETON | METAL_WA_LANG_VERSION_3_2;
    out_config->disabled_workarounds = 0;
    out_config->metal_language_version = 0x00030000u;  // Metal 3.0
    out_config->reserved = 0;
    return METAL_RESULT_OK;
}

metal_result metal_configure_shader_compiler(
    metal_shader_compiler* compiler,
    const metal_shader_compiler_config* config)
{
    if (compiler == NULL || config == NULL)
    {
        return METAL_RESULT_INVALID_ARGUMENT;
    }

    // P4.2.x：应用编译器配置
    return METAL_RESULT_UNSUPPORTED;
}

uint32_t metal_shader_compiler_get_workarounds(
    metal_shader_compiler* compiler)
{
    if (compiler == NULL)
    {
        return 0u;
    }
    // P4.2.x：返回当前 workaround 位掩码
    return 0u;
}

uint32_t metal_bridge_abi_version(void)
{
    return METAL_BRIDGE_ABI_VERSION;
}

void metal_release(void* handle)
{
    // P4.1.x + P4.2.x：按 handle 类型分发到对应析构函数
    (void)handle;
}

const char* metal_get_last_error_message(void)
{
    return NULL;
}

