// MetalSampler.cpp — Metal 采样器创建：过滤器/寻址/比较模式映射
//
// P4.1.4 完整实现。支持 min/mag/mip 过滤器、S/T/R 寻址模式、
// 比较函数、各向异性、LOD clamp、归一化坐标。

#include "metal_bridge.h"
#include "metal_internal.h"

#include <new>

// ════════════════════════════════════════════════════════════════════
// metal 枚举 → metal-cpp 枚举映射（内联辅助）
// ════════════════════════════════════════════════════════════════════

static MTL::SamplerMinMagFilter to_mtl_min_mag(metal_sampler_min_mag_filter filter)
{
    switch (filter)
    {
    case METAL_SAMPLER_FILTER_NEAREST: return MTL::SamplerMinMagFilterNearest;
    case METAL_SAMPLER_FILTER_LINEAR:  return MTL::SamplerMinMagFilterLinear;
    default:                           return MTL::SamplerMinMagFilterNearest;
    }
}

static MTL::SamplerMipFilter to_mtl_mip(metal_sampler_mip_filter filter)
{
    switch (filter)
    {
    case METAL_SAMPLER_MIP_FILTER_NOT_MIPMAPPED: return MTL::SamplerMipFilterNotMipmapped;
    case METAL_SAMPLER_MIP_FILTER_NEAREST:       return MTL::SamplerMipFilterNearest;
    case METAL_SAMPLER_MIP_FILTER_LINEAR:        return MTL::SamplerMipFilterLinear;
    default:                                     return MTL::SamplerMipFilterNotMipmapped;
    }
}

static MTL::SamplerAddressMode to_mtl_address(metal_sampler_address_mode mode)
{
    switch (mode)
    {
    case METAL_SAMPLER_ADDRESS_CLAMP_TO_EDGE:        return MTL::SamplerAddressModeClampToEdge;
    case METAL_SAMPLER_ADDRESS_REPEAT:               return MTL::SamplerAddressModeRepeat;
    case METAL_SAMPLER_ADDRESS_MIRRORED_REPEAT:      return MTL::SamplerAddressModeMirrorRepeat;
    case METAL_SAMPLER_ADDRESS_CLAMP_TO_ZERO:        return MTL::SamplerAddressModeClampToZero;
    case METAL_SAMPLER_ADDRESS_CLAMP_TO_BORDER_COLOR: return MTL::SamplerAddressModeClampToBorderColor;
    case METAL_SAMPLER_ADDRESS_MIRROR_CLAMP_TO_EDGE: return MTL::SamplerAddressModeMirrorClampToEdge;
    default:                                          return MTL::SamplerAddressModeClampToEdge;
    }
}

static MTL::CompareFunction to_mtl_compare(metal_compare_function func)
{
    switch (func)
    {
    case METAL_COMPARE_NEVER:          return MTL::CompareFunctionNever;
    case METAL_COMPARE_LESS:           return MTL::CompareFunctionLess;
    case METAL_COMPARE_EQUAL:          return MTL::CompareFunctionEqual;
    case METAL_COMPARE_LESS_EQUAL:     return MTL::CompareFunctionLessEqual;
    case METAL_COMPARE_GREATER:        return MTL::CompareFunctionGreater;
    case METAL_COMPARE_NOT_EQUAL:      return MTL::CompareFunctionNotEqual;
    case METAL_COMPARE_GREATER_EQUAL:  return MTL::CompareFunctionGreaterEqual;
    case METAL_COMPARE_ALWAYS:         return MTL::CompareFunctionAlways;
    default:                           return MTL::CompareFunctionAlways;
    }
}

// ════════════════════════════════════════════════════════════════════
// P4.1.4 — MetalSampler 创建
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_sampler(
    metal_device* device,
    const metal_sampler_descriptor* descriptor,
    metal_sampler** out_sampler)
{
    if (device == nullptr || descriptor == nullptr || out_sampler == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::SamplerDescriptor* mtl_desc = MTL::SamplerDescriptor::alloc()->init();
    if (mtl_desc == nullptr)
    {
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    // 设置过滤器
    mtl_desc->setMinFilter(to_mtl_min_mag(descriptor->min_filter));
    mtl_desc->setMagFilter(to_mtl_min_mag(descriptor->mag_filter));
    mtl_desc->setMipFilter(to_mtl_mip(descriptor->mip_filter));

    // 设置寻址模式
    mtl_desc->setSAddressMode(to_mtl_address(descriptor->address_s));
    mtl_desc->setTAddressMode(to_mtl_address(descriptor->address_t));
    mtl_desc->setRAddressMode(to_mtl_address(descriptor->address_r));

    // 设置比较函数（仅当 compare_function != ALWAYS 或 sampler 用于阴影时）
    mtl_desc->setCompareFunction(to_mtl_compare(descriptor->compare_function));

    // 设置各向异性
    if (descriptor->max_anisotropy > 0.0f)
        mtl_desc->setMaxAnisotropy(descriptor->max_anisotropy);

    // 设置 LOD clamp
    mtl_desc->setLodMinClamp(descriptor->lod_min_clamp);
    mtl_desc->setLodMaxClamp(descriptor->lod_max_clamp);

    // 设置归一化坐标
    mtl_desc->setNormalizedCoordinates(descriptor->normalized_coordinates);

    // 创建 MTL::SamplerState
    MTL::SamplerState* sampler_state = device->device->newSamplerState(mtl_desc);
    mtl_desc->release();

    if (sampler_state == nullptr)
    {
        pool->release();
        return METAL_RESULT_RUNTIME_ERROR;
    }

    metal_sampler* sampler = new (std::nothrow) metal_sampler();
    if (sampler == nullptr)
    {
        sampler_state->release();
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    sampler->base.type = METAL_HANDLE_TYPE_SAMPLER;
    sampler->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    sampler->sampler_state = sampler_state;

    pool->release();
    *out_sampler = sampler;
    return METAL_RESULT_OK;
}
