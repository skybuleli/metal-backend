using Ryujinx.Graphics.GAL;
using System;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    /// <summary>
    /// Ryujinx GAL 类型到 Metal 枚举的完整映射表。
    /// 覆盖 Format（170+ 值）→ MetalPixelFormat（53 值）、Target→MetalTextureType、Usage 推导。
    /// </summary>
    [SupportedOSPlatform("macos")]
    internal static class MetalFormatMapping
    {
        /// <summary>
        /// 将 GAL Format 映射到 MetalPixelFormat。
        /// 不支持的格式返回 Invalid，调用方应回退或报错。
        /// </summary>
        public static MetalPixelFormat ToMetalFormat(this Format format)
        {
            return format switch
            {
                // ── 8-bit ──
                Format.R8Unorm   => MetalPixelFormat.R8Unorm,
                Format.R8Snorm   => MetalPixelFormat.R8Snorm,
                Format.R8Uint    => MetalPixelFormat.R8Uint,
                Format.R8Sint    => MetalPixelFormat.R8Sint,

                // ── 16-bit ──
                Format.R16Float  => MetalPixelFormat.R16Float,
                Format.R16Unorm  => MetalPixelFormat.R16Unorm,
                Format.R16Snorm  => MetalPixelFormat.Invalid, // Metal 无 R16Snorm
                Format.R16Uint   => MetalPixelFormat.R16Uint,
                Format.R16Sint   => MetalPixelFormat.R16Sint,

                // ── 32-bit ──
                Format.R32Float  => MetalPixelFormat.R32Float,
                Format.R32Uint   => MetalPixelFormat.R32Uint,
                Format.R32Sint   => MetalPixelFormat.R32Sint,

                // ── RG 8-bit — Metal 仅支持 RG8Unorm ──
                Format.R8G8Unorm  => MetalPixelFormat.RG8Unorm,
                Format.R8G8Snorm  => MetalPixelFormat.Invalid,
                Format.R8G8Uint   => MetalPixelFormat.Invalid,
                Format.R8G8Sint   => MetalPixelFormat.Invalid,

                // ── RG 16-bit ──
                Format.R16G16Float => MetalPixelFormat.RG16Float,
                Format.R16G16Unorm => MetalPixelFormat.RG16Unorm,
                Format.R16G16Snorm => MetalPixelFormat.Invalid, // Metal 无 RG16Snorm
                Format.R16G16Uint  => MetalPixelFormat.RG16Uint,
                Format.R16G16Sint  => MetalPixelFormat.RG16Sint,

                // ── RG 32-bit ──
                Format.R32G32Float => MetalPixelFormat.RG32Float,
                Format.R32G32Uint  => MetalPixelFormat.RG32Uint,
                Format.R32G32Sint  => MetalPixelFormat.RG32Sint,

                // ── RGB 8-bit — Metal 无 3-channel ──
                Format.R8G8B8Unorm => MetalPixelFormat.Invalid,
                Format.R8G8B8Snorm => MetalPixelFormat.Invalid,
                Format.R8G8B8Uint  => MetalPixelFormat.Invalid,
                Format.R8G8B8Sint  => MetalPixelFormat.Invalid,

                // ── RGB 16-bit — Metal 无 3-channel ──
                Format.R16G16B16Float => MetalPixelFormat.Invalid,
                Format.R16G16B16Unorm => MetalPixelFormat.Invalid,
                Format.R16G16B16Snorm => MetalPixelFormat.Invalid,
                Format.R16G16B16Uint  => MetalPixelFormat.Invalid,
                Format.R16G16B16Sint  => MetalPixelFormat.Invalid,

                // ── RGB 32-bit — Metal 无 3-channel ──
                Format.R32G32B32Float => MetalPixelFormat.Invalid,
                Format.R32G32B32Uint  => MetalPixelFormat.Invalid,
                Format.R32G32B32Sint  => MetalPixelFormat.Invalid,

                // ── RGBA 8-bit ──
                Format.R8G8B8A8Unorm => MetalPixelFormat.RGBA8Unorm,
                Format.R8G8B8A8Snorm => MetalPixelFormat.RGBA8Snorm,
                Format.R8G8B8A8Uint  => MetalPixelFormat.RGBA8Uint,
                Format.R8G8B8A8Sint  => MetalPixelFormat.RGBA8Sint,
                Format.R8G8B8A8Srgb  => MetalPixelFormat.RGBA8SRgb,

                // ── RGBA 16-bit ──
                Format.R16G16B16A16Float => MetalPixelFormat.RGBA16Float,
                Format.R16G16B16A16Unorm => MetalPixelFormat.RGBA16Unorm,
                Format.R16G16B16A16Snorm => MetalPixelFormat.Invalid, // Metal 无 RGBA16Snorm
                Format.R16G16B16A16Uint  => MetalPixelFormat.RGBA16Uint,
                Format.R16G16B16A16Sint  => MetalPixelFormat.RGBA16Sint,

                // ── RGBA 32-bit ──
                Format.R32G32B32A32Float => MetalPixelFormat.RGBA32Float,
                Format.R32G32B32A32Uint  => MetalPixelFormat.RGBA32Uint,
                Format.R32G32B32A32Sint  => MetalPixelFormat.RGBA32Sint,

                // ── BGRA ──
                Format.B8G8R8A8Unorm => MetalPixelFormat.BGRA8Unorm,
                Format.B8G8R8A8Srgb  => MetalPixelFormat.BGRA8SRgb,

                // ── Packed ──
                Format.R10G10B10A2Unorm  => MetalPixelFormat.R10G10B10A2Unorm,
                Format.R10G10B10A2Uint   => MetalPixelFormat.Invalid, // Metal 需要 R10G10B10A2Unorm 只支持 unorm
                Format.R10G10B10A2Snorm  => MetalPixelFormat.Invalid,
                Format.R10G10B10A2Sint   => MetalPixelFormat.Invalid,
                Format.R10G10B10A2Uscaled => MetalPixelFormat.Invalid,
                Format.R10G10B10A2Sscaled => MetalPixelFormat.Invalid,
                Format.R11G11B10Float    => MetalPixelFormat.R11G11B10Float,
                Format.R9G9B9E5Float     => MetalPixelFormat.RGB9E5Float,
                Format.B10G10R10A2Unorm  => MetalPixelFormat.Invalid, // Metal 仅支持 R10G10B10A2
                Format.R4G4Unorm    => MetalPixelFormat.Invalid,
                Format.R4G4B4A4Unorm => MetalPixelFormat.RGBA8Unorm,   // 4→8 近似
                Format.R5G5B5X1Unorm => MetalPixelFormat.Invalid,
                Format.R5G5B5A1Unorm => MetalPixelFormat.Invalid,
                Format.R5G6B5Unorm   => MetalPixelFormat.Invalid,
                Format.B5G6R5Unorm   => MetalPixelFormat.Invalid,
                Format.B5G5R5A1Unorm => MetalPixelFormat.Invalid,
                Format.A1B5G5R5Unorm => MetalPixelFormat.Invalid,

                // ── Depth / Stencil ──
                Format.D16Unorm        => MetalPixelFormat.D16Unorm,
                Format.D32Float        => MetalPixelFormat.D32Float,
                Format.D24UnormS8Uint  => MetalPixelFormat.D24UnormS8Uint,
                Format.D32FloatS8Uint  => MetalPixelFormat.D32FloatS8Uint,
                Format.S8Uint          => MetalPixelFormat.D24UnormS8Uint, // stencil-only 映射到 Depth24_Stencil8
                Format.S8UintD24Unorm  => MetalPixelFormat.D24UnormS8Uint, // 相同布局
                Format.X8UintD24Unorm  => MetalPixelFormat.D24UnormS8Uint, // 带 padding，近似

                // ── Scaled — Metal 不支持 ──
                Format.R8Uscaled or Format.R8Sscaled
                    or Format.R16Uscaled or Format.R16Sscaled
                    or Format.R32Uscaled or Format.R32Sscaled
                    or Format.R8G8Uscaled or Format.R8G8Sscaled
                    or Format.R16G16Uscaled or Format.R16G16Sscaled
                    or Format.R32G32Uscaled or Format.R32G32Sscaled
                    or Format.R8G8B8Uscaled or Format.R8G8B8Sscaled
                    or Format.R16G16B16Uscaled or Format.R16G16B16Sscaled
                    or Format.R32G32B32Uscaled or Format.R32G32B32Sscaled
                    or Format.R8G8B8A8Uscaled or Format.R8G8B8A8Sscaled
                    or Format.R16G16B16A16Uscaled or Format.R16G16B16A16Sscaled
                    or Format.R32G32B32A32Uscaled or Format.R32G32B32A32Sscaled
                    => MetalPixelFormat.Invalid,

                // ── BC 压缩 ──
                Format.Bc1RgbaUnorm => MetalPixelFormat.BC1RGBA,
                Format.Bc1RgbaSrgb  => MetalPixelFormat.BC1RGBA, // sRGB → unorm（C++ 侧映射局限）
                Format.Bc2Unorm     => MetalPixelFormat.BC2RGBA,
                Format.Bc2Srgb      => MetalPixelFormat.BC2RGBA,
                Format.Bc3Unorm     => MetalPixelFormat.BC3RGBA,
                Format.Bc3Srgb      => MetalPixelFormat.BC3RGBA,
                Format.Bc4Unorm     => MetalPixelFormat.BC4R,
                Format.Bc4Snorm     => MetalPixelFormat.BC4R,      // Metal BC4 只有 unorm
                Format.Bc5Unorm     => MetalPixelFormat.BC5RG,
                Format.Bc5Snorm     => MetalPixelFormat.BC5RG,      // Metal BC5 只有 unorm
                Format.Bc6HSfloat   => MetalPixelFormat.BC6HRGB,
                Format.Bc6HUfloat   => MetalPixelFormat.BC6HRGB,
                Format.Bc7Unorm     => MetalPixelFormat.BC7RGBA,
                Format.Bc7Srgb      => MetalPixelFormat.BC7RGBA,

                // ── ETC2 ──
                Format.Etc2RgbUnorm   => MetalPixelFormat.Etc2Rgb,
                Format.Etc2RgbaUnorm  => MetalPixelFormat.Etc2Rgba,
                Format.Etc2RgbPtaUnorm => MetalPixelFormat.Etc2Rgb, // punch-through alpha → RGB
                Format.Etc2RgbSrgb    => MetalPixelFormat.Etc2Rgb,
                Format.Etc2RgbaSrgb   => MetalPixelFormat.Etc2Rgba,
                Format.Etc2RgbPtaSrgb => MetalPixelFormat.Etc2Rgb,

                // ── ASTC ──
                Format.Astc4x4Unorm  => MetalPixelFormat.Astc4x4Ldr,
                Format.Astc4x4Srgb   => MetalPixelFormat.Astc4x4Ldr,
                Format.Astc5x4Unorm  => MetalPixelFormat.Invalid, // Metal 无 5x4
                Format.Astc5x5Unorm  => MetalPixelFormat.Invalid, // Metal 无 5x5
                Format.Astc6x5Unorm  => MetalPixelFormat.Invalid, // Metal 无 6x5
                Format.Astc6x6Unorm  => MetalPixelFormat.Astc6x6Ldr,
                Format.Astc6x6Srgb   => MetalPixelFormat.Astc6x6Ldr,
                Format.Astc8x5Unorm  => MetalPixelFormat.Invalid,
                Format.Astc8x6Unorm  => MetalPixelFormat.Invalid,
                Format.Astc8x8Unorm  => MetalPixelFormat.Astc8x8Ldr,
                Format.Astc8x8Srgb   => MetalPixelFormat.Astc8x8Ldr,
                Format.Astc10x5Unorm => MetalPixelFormat.Invalid,
                Format.Astc10x6Unorm => MetalPixelFormat.Invalid,
                Format.Astc10x8Unorm => MetalPixelFormat.Invalid,
                Format.Astc10x10Unorm => MetalPixelFormat.Invalid,
                Format.Astc12x10Unorm => MetalPixelFormat.Invalid,
                Format.Astc12x12Unorm => MetalPixelFormat.Astc12x12Ldr,
                Format.Astc12x12Srgb  => MetalPixelFormat.Astc12x12Ldr,
                Format.Astc5x4Srgb   => MetalPixelFormat.Invalid,
                Format.Astc5x5Srgb   => MetalPixelFormat.Invalid,
                Format.Astc6x5Srgb   => MetalPixelFormat.Invalid,
                Format.Astc8x5Srgb   => MetalPixelFormat.Invalid,
                Format.Astc8x6Srgb   => MetalPixelFormat.Invalid,
                Format.Astc10x5Srgb  => MetalPixelFormat.Invalid,
                Format.Astc10x6Srgb  => MetalPixelFormat.Invalid,
                Format.Astc10x8Srgb  => MetalPixelFormat.Invalid,
                Format.Astc10x10Srgb => MetalPixelFormat.Invalid,

                // ── 其他 BGR packed ──
                Format.A8B8G8R8Uint  => MetalPixelFormat.Invalid,

                _ => MetalPixelFormat.Invalid,
            };
        }

        /// <summary>
        /// 将 GAL Target 映射到 MetalTextureType。
        /// </summary>
        public static MetalTextureType ToMetalTextureType(this Target target)
        {
            return target switch
            {
                Target.Texture1D => MetalTextureType.Type2D,               // Metal 无 1D，用 2D 模拟
                Target.Texture2D => MetalTextureType.Type2D,
                Target.Texture3D => MetalTextureType.Type3D,
                Target.Texture1DArray => MetalTextureType.Type2DArray,     // 1D array → 2D array
                Target.Texture2DArray => MetalTextureType.Type2DArray,
                Target.Texture2DMultisample => MetalTextureType.Type2DMultisample,
                Target.Texture2DMultisampleArray => MetalTextureType.Type2DMultisample,
                Target.Cubemap => MetalTextureType.Cube,
                Target.CubemapArray => MetalTextureType.Type2DArray,       // Metal 无 CubeArray，用 2DArray 模拟
                Target.TextureBuffer => MetalTextureType.Type2D,           // 回退到 2D
                _ => MetalTextureType.Type2D,
            };
        }

        /// <summary>
        /// 推导纹理使用标志位。
        /// 安全默认：所有纹理解锁 ShaderRead，渲染目标附加 RenderTarget。
        /// </summary>
        public static MetalTextureUsage DeriveTextureUsage(this TextureCreateInfo info)
        {
            MetalTextureUsage usage = MetalTextureUsage.ShaderRead;

            // 颜色可渲染格式附加 RenderTarget
            if (info.Format.IsRtColorCompatible || info.Format.HasDepth)
            {
                usage |= MetalTextureUsage.RenderTarget;
            }

            // 图像兼容格式（可被 compute shader 写入）附加 ShaderWrite
            if (info.Format.IsImageCompatible)
            {
                usage |= MetalTextureUsage.ShaderWrite;
            }

            return usage;
        }
    }
}
