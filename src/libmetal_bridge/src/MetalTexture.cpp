// MetalTexture.cpp — Metal 纹理管理：创建、上传、回读、信息查询
//
// 本文件实现 P4.1.3 的全部纹理 C ABI 函数：
//   - metal_create_texture         — 从描述参数创建 MTLTexture
//   - metal_texture_get_info       — 查询纹理元信息
//   - metal_texture_upload         — 通过缓冲区上传像素数据（replaceRegion）
//   - metal_texture_readback       — 回读纹理像素数据（getBytes）
//   - metal_pixel_format_get_info  — 查询像素格式的字节数/块尺寸/类型
//
// 设计要点：
//   1. 格式映射表覆盖 ~50 种常用 GAL 像素格式 → MTLPixelFormat
//   2. 所有 metal-cpp 调用在 AutoreleasePool 中执行
//   3. 上传/回读通过中间 metal_buffer（Shared 模式）中转

#include "metal_bridge.h"
#include "metal_limits.h"
#include "metal_internal.h"

#include <cstring>
#include <cstdint>
#include <new>

// ════════════════════════════════════════════════════════════════════
// 内部辅助：metal_pixel_format → MTL::PixelFormat 映射表
// ════════════════════════════════════════════════════════════════════

/// 核心映射函数：将 C ABI 的 metal_pixel_format 转换为 MTL::PixelFormat
static MTL::PixelFormat to_mtl_pixel_format(metal_pixel_format fmt)
{
    switch (fmt)
    {
    // 8-bit
    case METAL_PIXEL_FORMAT_R8_UNORM:   return MTL::PixelFormatR8Unorm;
    case METAL_PIXEL_FORMAT_R8_SNORM:   return MTL::PixelFormatR8Snorm;
    case METAL_PIXEL_FORMAT_R8_UINT:    return MTL::PixelFormatR8Uint;
    case METAL_PIXEL_FORMAT_R8_SINT:    return MTL::PixelFormatR8Sint;
    // 16-bit
    case METAL_PIXEL_FORMAT_R16_FLOAT:  return MTL::PixelFormatR16Float;
    case METAL_PIXEL_FORMAT_R16_UNORM:  return MTL::PixelFormatR16Unorm;
    case METAL_PIXEL_FORMAT_R16_UINT:   return MTL::PixelFormatR16Uint;
    case METAL_PIXEL_FORMAT_R16_SINT:   return MTL::PixelFormatR16Sint;
    case METAL_PIXEL_FORMAT_RG8_UNORM:  return MTL::PixelFormatRG8Unorm;
    // 32-bit
    case METAL_PIXEL_FORMAT_R32_FLOAT:  return MTL::PixelFormatR32Float;
    case METAL_PIXEL_FORMAT_R32_UINT:   return MTL::PixelFormatR32Uint;
    case METAL_PIXEL_FORMAT_R32_SINT:   return MTL::PixelFormatR32Sint;
    case METAL_PIXEL_FORMAT_RG16_FLOAT: return MTL::PixelFormatRG16Float;
    case METAL_PIXEL_FORMAT_RG16_UNORM: return MTL::PixelFormatRG16Unorm;
    case METAL_PIXEL_FORMAT_RG16_UINT:  return MTL::PixelFormatRG16Uint;
    case METAL_PIXEL_FORMAT_RG16_SINT:  return MTL::PixelFormatRG16Sint;
    case METAL_PIXEL_FORMAT_RGBA8_UNORM: return MTL::PixelFormatRGBA8Unorm;
    case METAL_PIXEL_FORMAT_RGBA8_SNORM: return MTL::PixelFormatRGBA8Snorm;
    case METAL_PIXEL_FORMAT_RGBA8_UINT:  return MTL::PixelFormatRGBA8Uint;
    case METAL_PIXEL_FORMAT_RGBA8_SINT:  return MTL::PixelFormatRGBA8Sint;
    case METAL_PIXEL_FORMAT_RGBA8_SRGB:  return MTL::PixelFormatRGBA8Unorm_sRGB;
    case METAL_PIXEL_FORMAT_BGRA8_UNORM: return MTL::PixelFormatBGRA8Unorm;
    case METAL_PIXEL_FORMAT_BGRA8_SRGB:  return MTL::PixelFormatBGRA8Unorm_sRGB;
    // 64-bit
    case METAL_PIXEL_FORMAT_RG32_FLOAT:  return MTL::PixelFormatRG32Float;
    case METAL_PIXEL_FORMAT_RG32_UINT:   return MTL::PixelFormatRG32Uint;
    case METAL_PIXEL_FORMAT_RG32_SINT:   return MTL::PixelFormatRG32Sint;
    case METAL_PIXEL_FORMAT_RGBA16_FLOAT: return MTL::PixelFormatRGBA16Float;
    case METAL_PIXEL_FORMAT_RGBA16_UNORM: return MTL::PixelFormatRGBA16Unorm;
    case METAL_PIXEL_FORMAT_RGBA16_UINT:  return MTL::PixelFormatRGBA16Uint;
    case METAL_PIXEL_FORMAT_RGBA16_SINT:  return MTL::PixelFormatRGBA16Sint;
    // 128-bit
    case METAL_PIXEL_FORMAT_RGBA32_FLOAT: return MTL::PixelFormatRGBA32Float;
    case METAL_PIXEL_FORMAT_RGBA32_UINT:  return MTL::PixelFormatRGBA32Uint;
    case METAL_PIXEL_FORMAT_RGBA32_SINT:  return MTL::PixelFormatRGBA32Sint;
    // Packed
    case METAL_PIXEL_FORMAT_R10G10B10A2_UNORM: return MTL::PixelFormatRGB10A2Unorm;
    case METAL_PIXEL_FORMAT_R11G11B10_FLOAT:   return MTL::PixelFormatRG11B10Float;
    case METAL_PIXEL_FORMAT_RGB9E5_FLOAT:      return MTL::PixelFormatRGB9E5Float;
    // Depth / Stencil
    case METAL_PIXEL_FORMAT_D16_UNORM:       return MTL::PixelFormatDepth16Unorm;
    case METAL_PIXEL_FORMAT_D32_FLOAT:       return MTL::PixelFormatDepth32Float;
    case METAL_PIXEL_FORMAT_D24_UNORM_S8_UINT: return MTL::PixelFormatDepth24Unorm_Stencil8;
    case METAL_PIXEL_FORMAT_D32_FLOAT_S8_UINT: return MTL::PixelFormatDepth32Float_Stencil8;
    // Compressed (BC)
    case METAL_PIXEL_FORMAT_BC1_RGBA: return MTL::PixelFormatBC1_RGBA;
    case METAL_PIXEL_FORMAT_BC2_RGBA: return MTL::PixelFormatBC2_RGBA;
    case METAL_PIXEL_FORMAT_BC3_RGBA: return MTL::PixelFormatBC3_RGBA;
    case METAL_PIXEL_FORMAT_BC4_R:    return MTL::PixelFormatBC4_RUnorm;
    case METAL_PIXEL_FORMAT_BC5_RG:   return MTL::PixelFormatBC5_RGUnorm;
    case METAL_PIXEL_FORMAT_BC6H_RGB: return MTL::PixelFormatBC6H_RGBUfloat;
    case METAL_PIXEL_FORMAT_BC7_RGBA: return MTL::PixelFormatBC7_RGBAUnorm;
    // Compressed (ASTC)
    case METAL_PIXEL_FORMAT_ASTC_4x4_LDR:  return MTL::PixelFormatASTC_4x4_LDR;
    case METAL_PIXEL_FORMAT_ASTC_6x6_LDR:  return MTL::PixelFormatASTC_6x6_LDR;
    case METAL_PIXEL_FORMAT_ASTC_8x8_LDR:  return MTL::PixelFormatASTC_8x8_LDR;
    case METAL_PIXEL_FORMAT_ASTC_12x12_LDR: return MTL::PixelFormatASTC_12x12_LDR;
    // ETC2 — Metal 不原生支持，展开到 RGBA8（软件转换由 C# 侧处理）
    case METAL_PIXEL_FORMAT_ETC2_RGB:
    case METAL_PIXEL_FORMAT_ETC2_RGBA:
        return MTL::PixelFormatRGBA8Unorm;
    default:
        return MTL::PixelFormatInvalid;
    }
}

/// 将 metal_texture_type 转换为 MTL::TextureType
static MTL::TextureType to_mtl_texture_type(metal_texture_type type)
{
    switch (type)
    {
    case METAL_TEXTURE_TYPE_2D:              return MTL::TextureType2D;
    case METAL_TEXTURE_TYPE_2D_ARRAY:        return MTL::TextureType2DArray;
    case METAL_TEXTURE_TYPE_CUBE:            return MTL::TextureTypeCube;
    case METAL_TEXTURE_TYPE_3D:              return MTL::TextureType3D;
    case METAL_TEXTURE_TYPE_2D_MULTISAMPLE:  return MTL::TextureType2DMultisample;
    default:                                 return MTL::TextureType2D;
    }
}

/// 将 metal_texture_usage 位掩码转换为 MTL::TextureUsage
static MTL::TextureUsage to_mtl_texture_usage(uint32_t usage_flags)
{
    MTL::TextureUsage usage = MTL::TextureUsageUnknown;
    if (usage_flags & METAL_TEXTURE_USAGE_SHADER_READ)
        usage |= MTL::TextureUsageShaderRead;
    if (usage_flags & METAL_TEXTURE_USAGE_SHADER_WRITE)
        usage |= MTL::TextureUsageShaderWrite;
    if (usage_flags & METAL_TEXTURE_USAGE_RENDER_TARGET)
        usage |= MTL::TextureUsageRenderTarget;
    if (usage_flags & METAL_TEXTURE_USAGE_PIXEL_FORMAT_VIEW)
        usage |= MTL::TextureUsagePixelFormatView;
    return usage;
}

/// 将 metal_storage_mode 转为 MTL::StorageMode（与 MetalBuffer.cpp 中一致）
static MTL::StorageMode to_mtl_storage_mode(metal_storage_mode mode)
{
    switch (mode)
    {
    case METAL_STORAGE_MODE_SHARED:     return MTL::StorageModeShared;
    case METAL_STORAGE_MODE_MANAGED:    return MTL::StorageModeManaged;
    case METAL_STORAGE_MODE_PRIVATE:    return MTL::StorageModePrivate;
    case METAL_STORAGE_MODE_MEMORYLESS: return MTL::StorageModeMemoryless;
    default:                            return MTL::StorageModeShared;
    }
}

// ════════════════════════════════════════════════════════════════════
// 像素格式元信息表
// ════════════════════════════════════════════════════════════════════

/// 格式元信息条目
struct FormatInfoEntry
{
    metal_pixel_format fmt;
    const char*        name;
    uint32_t           bytes_per_pixel;
    uint32_t           block_width;
    uint32_t           block_height;
    bool               is_depth;
    bool               is_compressed;
};

/// 格式元信息查找表（按 metal_pixel_format 枚举值排列）
/// bytes_per_pixel 为 0 表示该格式使用 block 压缩（参考 block_width/block_height）
static constexpr FormatInfoEntry kFormatInfoTable[] = {
    // 8-bit
    {METAL_PIXEL_FORMAT_R8_UNORM,   "R8Unorm",   1, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R8_SNORM,   "R8Snorm",   1, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R8_UINT,    "R8Uint",    1, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R8_SINT,    "R8Sint",    1, 1, 1, false, false},
    // 16-bit
    {METAL_PIXEL_FORMAT_R16_FLOAT,  "R16Float",  2, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R16_UNORM,  "R16Unorm",  2, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R16_UINT,   "R16Uint",   2, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R16_SINT,   "R16Sint",   2, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RG8_UNORM,  "RG8Unorm",  2, 1, 1, false, false},
    // 32-bit
    {METAL_PIXEL_FORMAT_R32_FLOAT,  "R32Float",  4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R32_UINT,   "R32Uint",   4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R32_SINT,   "R32Sint",   4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RG16_FLOAT, "RG16Float", 4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RG16_UNORM, "RG16Unorm", 4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RG16_UINT,  "RG16Uint",  4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RG16_SINT,  "RG16Sint",  4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA8_UNORM, "RGBA8Unorm", 4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA8_SNORM, "RGBA8Snorm", 4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA8_UINT,  "RGBA8Uint",  4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA8_SINT,  "RGBA8Sint",  4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA8_SRGB,  "RGBA8SRgb",  4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_BGRA8_UNORM, "BGRA8Unorm", 4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_BGRA8_SRGB,  "BGRA8SRgb",  4, 1, 1, false, false},
    // 64-bit
    {METAL_PIXEL_FORMAT_RG32_FLOAT,  "RG32Float",  8, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RG32_UINT,   "RG32Uint",   8, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RG32_SINT,   "RG32Sint",   8, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA16_FLOAT, "RGBA16Float", 8, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA16_UNORM, "RGBA16Unorm", 8, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA16_UINT,  "RGBA16Uint",  8, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA16_SINT,  "RGBA16Sint",  8, 1, 1, false, false},
    // 128-bit
    {METAL_PIXEL_FORMAT_RGBA32_FLOAT, "RGBA32Float", 16, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA32_UINT,  "RGBA32Uint",  16, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGBA32_SINT,  "RGBA32Sint",  16, 1, 1, false, false},
    // Packed
    {METAL_PIXEL_FORMAT_R10G10B10A2_UNORM, "RGB10A2Unorm", 4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_R11G11B10_FLOAT,   "RG11B10Float", 4, 1, 1, false, false},
    {METAL_PIXEL_FORMAT_RGB9E5_FLOAT,      "RGB9E5Float",  4, 1, 1, false, false},
    // Depth / Stencil
    {METAL_PIXEL_FORMAT_D16_UNORM,        "Depth16Unorm",       2, 1, 1, true, false},
    {METAL_PIXEL_FORMAT_D32_FLOAT,        "Depth32Float",       4, 1, 1, true, false},
    {METAL_PIXEL_FORMAT_D24_UNORM_S8_UINT, "Depth24UnormStencil8", 4, 1, 1, true, false},
    {METAL_PIXEL_FORMAT_D32_FLOAT_S8_UINT, "Depth32FloatStencil8", 8, 1, 1, true, false},
    // Compressed (BC)
    {METAL_PIXEL_FORMAT_BC1_RGBA, "BC1_RGBA",      0, 4, 4, false, true},
    {METAL_PIXEL_FORMAT_BC2_RGBA, "BC2_RGBA",      0, 4, 4, false, true},
    {METAL_PIXEL_FORMAT_BC3_RGBA, "BC3_RGBA",      0, 4, 4, false, true},
    {METAL_PIXEL_FORMAT_BC4_R,    "BC4_RUnorm",    0, 4, 4, false, true},
    {METAL_PIXEL_FORMAT_BC5_RG,   "BC5_RGUnorm",   0, 4, 4, false, true},
    {METAL_PIXEL_FORMAT_BC6H_RGB, "BC6H_RGBUfloat", 0, 4, 4, false, true},
    {METAL_PIXEL_FORMAT_BC7_RGBA, "BC7_RGBAUnorm", 0, 4, 4, false, true},
    // Compressed (ASTC)
    {METAL_PIXEL_FORMAT_ASTC_4x4_LDR,  "ASTC_4x4_LDR",  0, 4, 4, false, true},
    {METAL_PIXEL_FORMAT_ASTC_6x6_LDR,  "ASTC_6x6_LDR",  0, 6, 6, false, true},
    {METAL_PIXEL_FORMAT_ASTC_8x8_LDR,  "ASTC_8x8_LDR",  0, 8, 8, false, true},
    {METAL_PIXEL_FORMAT_ASTC_12x12_LDR, "ASTC_12x12_LDR", 0, 12, 12, false, true},
};

static constexpr size_t kFormatInfoCount = sizeof(kFormatInfoTable) / sizeof(kFormatInfoTable[0]);

/// 在格式信息表中查找指定格式的条目
static const FormatInfoEntry* find_format_info(metal_pixel_format fmt)
{
    for (size_t i = 0; i < kFormatInfoCount; ++i)
    {
        if (kFormatInfoTable[i].fmt == fmt)
            return &kFormatInfoTable[i];
    }
    return nullptr;
}

// ════════════════════════════════════════════════════════════════════
// 分配并初始化 metal_texture 内部结构体
// ════════════════════════════════════════════════════════════════════

static metal_texture* allocate_texture_struct(
    MTL::Texture* mtl_texture,
    uint32_t width,
    uint32_t height,
    uint32_t depth,
    uint32_t levels,
    uint32_t samples,
    metal_texture_type type,
    metal_pixel_format pixel_format,
    metal_storage_mode storage_mode,
    metal_result* out_result)
{
    if (mtl_texture == nullptr)
    {
        *out_result = METAL_RESULT_OUT_OF_MEMORY;
        return nullptr;
    }

    metal_texture* tex = new (std::nothrow) metal_texture();
    if (tex == nullptr)
    {
        mtl_texture->release();
        *out_result = METAL_RESULT_OUT_OF_MEMORY;
        return nullptr;
    }

    tex->base.type = METAL_HANDLE_TYPE_TEXTURE;
    tex->base.abi_version = METAL_BRIDGE_ABI_VERSION;
    tex->texture = mtl_texture;
    tex->width = width;
    tex->height = height;
    tex->depth = depth;
    tex->levels = levels;
    tex->samples = samples;
    tex->type = type;
    tex->pixel_format = pixel_format;
    tex->storage_mode = storage_mode;

    *out_result = METAL_RESULT_OK;
    return tex;
}

// ════════════════════════════════════════════════════════════════════
// 像素格式信息查询
// ════════════════════════════════════════════════════════════════════

metal_pixel_format_info metal_pixel_format_get_info(
    metal_pixel_format format)
{
    metal_pixel_format_info info;
    std::memset(&info, 0, sizeof(info));

    const FormatInfoEntry* entry = find_format_info(format);
    if (entry == nullptr)
    {
        // 无效格式返回全零
        info.name[0] = '\0';
        info.bytes_per_pixel = 0;
        info.block_width = 1;
        info.block_height = 1;
        info.is_depth = false;
        info.is_compressed = false;
        info.is_srgb = false;
        info.reserved = 0;
        return info;
    }

    std::strncpy(info.name, entry->name, METAL_PIXEL_FORMAT_NAME_MAX - 1);
    info.name[METAL_PIXEL_FORMAT_NAME_MAX - 1] = '\0';
    info.bytes_per_pixel = entry->bytes_per_pixel;
    info.block_width = entry->block_width;
    info.block_height = entry->block_height;
    info.is_depth = entry->is_depth;
    info.is_compressed = entry->is_compressed;

    // 判断是否为 sRGB 格式（通过格式名称检查）
    info.is_srgb = (format == METAL_PIXEL_FORMAT_RGBA8_SRGB ||
                    format == METAL_PIXEL_FORMAT_BGRA8_SRGB);
    info.reserved = 0;

    return info;
}

// ════════════════════════════════════════════════════════════════════
// 创建纹理
// ════════════════════════════════════════════════════════════════════

metal_result metal_create_texture(
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
    metal_texture** out_texture)
{
    if (device == nullptr || out_texture == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (device->device == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    if (width == 0 || height == 0 || depth == 0)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (levels == 0)
        levels = 1;

    if (samples == 0)
        samples = 1;

    // 验证像素格式
    MTL::PixelFormat mtl_format = to_mtl_pixel_format(format);
    if (mtl_format == MTL::PixelFormatInvalid)
        return METAL_RESULT_INVALID_ARGUMENT;

    // 验证纹理尺寸不超限制
    if (type == METAL_TEXTURE_TYPE_3D)
    {
        if (width > METAL_MAX_3D_TEXTURE_DIM ||
            height > METAL_MAX_3D_TEXTURE_DIM ||
            depth > METAL_MAX_3D_TEXTURE_DIM)
            return METAL_RESULT_INVALID_ARGUMENT;
    }
    else if (type == METAL_TEXTURE_TYPE_CUBE)
    {
        if (width > METAL_MAX_CUBE_TEXTURE_DIM ||
            height > METAL_MAX_CUBE_TEXTURE_DIM)
            return METAL_RESULT_INVALID_ARGUMENT;
    }
    else
    {
        if (width > METAL_MAX_2D_TEXTURE_DIM ||
            height > METAL_MAX_2D_TEXTURE_DIM)
            return METAL_RESULT_INVALID_ARGUMENT;
    }

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    MTL::TextureDescriptor* desc = MTL::TextureDescriptor::alloc()->init();
    desc->setPixelFormat(mtl_format);
    desc->setWidth(static_cast<NS::UInteger>(width));
    desc->setHeight(static_cast<NS::UInteger>(height));

    if (type == METAL_TEXTURE_TYPE_3D)
    {
        desc->setDepth(static_cast<NS::UInteger>(depth));
    }
    else
    {
        desc->setDepth(static_cast<NS::UInteger>(depth > 1 ? depth : 1));
    }

    desc->setMipmapLevelCount(static_cast<NS::UInteger>(levels));
    desc->setSampleCount(static_cast<NS::UInteger>(samples));
    desc->setTextureType(to_mtl_texture_type(type));
    desc->setUsage(to_mtl_texture_usage(usage_flags));
    desc->setStorageMode(to_mtl_storage_mode(storage_mode));

    // 对于 Array 和 Cube 类型，设置 arrayLength
    if (type == METAL_TEXTURE_TYPE_2D_ARRAY)
    {
        desc->setArrayLength(static_cast<NS::UInteger>(depth > 1 ? depth : 1));
    }
    else if (type == METAL_TEXTURE_TYPE_CUBE)
    {
        // Cube 纹理的 arrayLength 为 6（每个面一层）
        // Metal 的 cube 纹理总是有 6 个 face（1 array slice = 6 faces）
        desc->setArrayLength(1);
    }

    MTL::Texture* mtl_texture = device->device->newTexture(desc);
    desc->release();

    if (mtl_texture == nullptr)
    {
        pool->release();
        return METAL_RESULT_OUT_OF_MEMORY;
    }

    metal_result result;
    metal_texture* tex = allocate_texture_struct(
        mtl_texture, width, height, depth, levels, samples,
        type, format, storage_mode, &result);

    pool->release();

    if (tex == nullptr)
        return result;

    *out_texture = tex;
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// 纹理信息查询
// ════════════════════════════════════════════════════════════════════

metal_result metal_texture_get_info(
    metal_texture* texture,
    metal_texture_info* out_info)
{
    if (texture == nullptr || out_info == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (texture->base.type != METAL_HANDLE_TYPE_TEXTURE)
        return METAL_RESULT_INVALID_ARGUMENT;

    out_info->width = texture->width;
    out_info->height = texture->height;
    out_info->depth = texture->depth;
    out_info->levels = texture->levels;
    out_info->samples = texture->samples;
    out_info->type = texture->type;
    out_info->pixel_format = texture->pixel_format;
    out_info->storage_mode = texture->storage_mode;
    out_info->reserved = 0;

    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// 上传纹理数据（buffer → texture）
// ════════════════════════════════════════════════════════════════════

metal_result metal_texture_upload(
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
    uint32_t bytes_per_row)
{
    if (texture == nullptr || buffer == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (texture->base.type != METAL_HANDLE_TYPE_TEXTURE)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->base.type != METAL_HANDLE_TYPE_BUFFER)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (texture->texture == nullptr || buffer->buffer == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // 验证 mip level 范围
    if (level >= texture->levels)
        return METAL_RESULT_INVALID_ARGUMENT;

    // 计算层索引
    NS::UInteger slice = 0;
    if (texture->type == METAL_TEXTURE_TYPE_CUBE)
    {
        // Cube 纹理：layer 是 face 索引（0-5）
        if (layer > 5)
            return METAL_RESULT_INVALID_ARGUMENT;
        slice = static_cast<NS::UInteger>(layer);
    }
    else if (texture->type == METAL_TEXTURE_TYPE_2D_ARRAY)
    {
        // Array 纹理：layer 是数组索引
        if (layer >= texture->depth)
            return METAL_RESULT_INVALID_ARGUMENT;
        slice = static_cast<NS::UInteger>(layer);
    }

    // 验证 region
    if (region_x + region_width > texture->width ||
        region_y + region_height > texture->height)
        return METAL_RESULT_INVALID_ARGUMENT;

    MTL::Region mtl_region = MTL::Region::Make3D(
        static_cast<NS::UInteger>(region_x),
        static_cast<NS::UInteger>(region_y),
        static_cast<NS::UInteger>(region_z),
        static_cast<NS::UInteger>(region_width),
        static_cast<NS::UInteger>(region_height),
        1); // 深度方向一次只上传一层

    NS::UInteger mtl_bytes_per_row = static_cast<NS::UInteger>(bytes_per_row);

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    // 获取源数据指针
    void* src_ptr = static_cast<uint8_t*>(buffer->buffer->contents()) +
                    static_cast<ptrdiff_t>(buffer_offset);

    texture->texture->replaceRegion(
        mtl_region,
        static_cast<NS::UInteger>(level),
        static_cast<NS::UInteger>(slice),
        src_ptr,
        mtl_bytes_per_row,
        0); // bytes_per_image（2D 纹理设为 0）

    pool->release();
    return METAL_RESULT_OK;
}

// ════════════════════════════════════════════════════════════════════
// 回读纹理数据（texture → buffer）
// ════════════════════════════════════════════════════════════════════

metal_result metal_texture_readback(
    metal_texture* texture,
    metal_buffer* buffer,
    uint64_t buffer_offset,
    uint32_t layer,
    uint32_t level,
    uint32_t bytes_per_row)
{
    if (texture == nullptr || buffer == nullptr)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (texture->base.type != METAL_HANDLE_TYPE_TEXTURE)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (buffer->base.type != METAL_HANDLE_TYPE_BUFFER)
        return METAL_RESULT_INVALID_ARGUMENT;

    if (texture->texture == nullptr || buffer->buffer == nullptr)
        return METAL_RESULT_RUNTIME_ERROR;

    // 验证 mip level 和 layer 范围
    if (level >= texture->levels)
        return METAL_RESULT_INVALID_ARGUMENT;

    // 计算层索引
    NS::UInteger slice = 0;
    if (texture->type == METAL_TEXTURE_TYPE_CUBE)
    {
        if (layer > 5)
            return METAL_RESULT_INVALID_ARGUMENT;
        slice = static_cast<NS::UInteger>(layer);
    }
    else if (texture->type == METAL_TEXTURE_TYPE_2D_ARRAY)
    {
        if (layer >= texture->depth)
            return METAL_RESULT_INVALID_ARGUMENT;
        slice = static_cast<NS::UInteger>(layer);
    }

    // 计算对应 mip 级别的尺寸
    uint32_t mip_width = texture->width >> level;
    uint32_t mip_height = texture->height >> level;
    if (mip_width < 1) mip_width = 1;
    if (mip_height < 1) mip_height = 1;

    MTL::Region mtl_region = MTL::Region::Make2D(0, 0,
        static_cast<NS::UInteger>(mip_width),
        static_cast<NS::UInteger>(mip_height));

    NS::UInteger mtl_bytes_per_row = static_cast<NS::UInteger>(bytes_per_row);

    NS::AutoreleasePool* pool = NS::AutoreleasePool::alloc()->init();

    // 获取目标指针
    void* dst_ptr = static_cast<uint8_t*>(buffer->buffer->contents()) +
                    static_cast<ptrdiff_t>(buffer_offset);

    // bytesPerImage: 对于 2D 纹理传入 0（等同于 bytes_per_row * height）
    // 对于 3D/array 纹理，这个值是每张 image 的字节数
    const NS::UInteger kBytesPerImage = 0;

    texture->texture->getBytes(
        dst_ptr,
        mtl_bytes_per_row,
        kBytesPerImage,
        mtl_region,
        static_cast<NS::UInteger>(level),
        static_cast<NS::UInteger>(slice));

    pool->release();
    return METAL_RESULT_OK;
}
