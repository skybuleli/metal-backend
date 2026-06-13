using System;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    internal static partial class MetalNative
    {
        private const string LibraryName = "libmetal_bridge.dylib";

        internal const uint AbiVersion = 1;

        [DllImport(LibraryName, EntryPoint = "metal_bridge_abi_version")]
        internal static extern uint BridgeAbiVersion();

        [DllImport(LibraryName, EntryPoint = "metal_release")]
        internal static extern void Release(nint handle);

        [DllImport(LibraryName, EntryPoint = "metal_get_last_error_message")]
        internal static extern nint GetLastErrorMessage();

        [DllImport(LibraryName, EntryPoint = "metal_create_device")]
        internal static extern MetalResult CreateDevice(out nint device);

        [DllImport(LibraryName, EntryPoint = "metal_get_device_info")]
        internal static extern MetalResult GetDeviceInfo(nint device, out MetalHandleInfo info);

        [DllImport(LibraryName, EntryPoint = "metal_get_device_caps")]
        internal static extern MetalResult GetDeviceCaps(nint device, out MetalDeviceCaps caps);

        [DllImport(LibraryName, EntryPoint = "metal_create_queue")]
        internal static extern MetalResult CreateQueue(nint device, out nint queue);

        [DllImport(LibraryName, EntryPoint = "metal_acquire_shader_compiler")]
        internal static extern MetalResult AcquireShaderCompiler(nint device, out nint compiler);

        [DllImport(LibraryName, EntryPoint = "metal_get_default_shader_compiler_config")]
        internal static extern MetalResult GetDefaultShaderCompilerConfig(out MetalShaderCompilerConfig config);

        [DllImport(LibraryName, EntryPoint = "metal_configure_shader_compiler")]
        internal static extern MetalResult ConfigureShaderCompiler(nint compiler, in MetalShaderCompilerConfig config);

        [DllImport(LibraryName, EntryPoint = "metal_shader_compiler_get_workarounds")]
        internal static extern uint ShaderCompilerGetWorkarounds(nint compiler);

        // ── Buffer P/Invoke (P4.1.2) ──
        [DllImport(LibraryName, EntryPoint = "metal_create_buffer")]
        internal static extern MetalResult CreateBuffer(nint device, ulong size, MetalStorageMode mode, out nint buffer);

        [DllImport(LibraryName, EntryPoint = "metal_create_buffer_with_bytes")]
        internal static extern MetalResult CreateBufferWithBytes(nint device, nint data, ulong size, MetalStorageMode mode, out nint buffer);

        [DllImport(LibraryName, EntryPoint = "metal_buffer_get_info")]
        internal static extern MetalResult BufferGetInfo(nint buffer, out MetalBufferInfo info);

        [DllImport(LibraryName, EntryPoint = "metal_map_buffer")]
        internal static extern MetalResult MapBuffer(nint buffer, out nint ptr);

        [DllImport(LibraryName, EntryPoint = "metal_unmap_buffer")]
        internal static extern MetalResult UnmapBuffer(nint buffer);

        [DllImport(LibraryName, EntryPoint = "metal_flush_buffer")]
        internal static extern MetalResult FlushBuffer(nint buffer, ulong offset, ulong size);

        [DllImport(LibraryName, EntryPoint = "metal_create_buffer_from_pointer")]
        internal static extern MetalResult CreateBufferFromPointer(nint device, nint ptr, ulong size, MetalStorageMode mode, out nint buffer);

        [DllImport(LibraryName, EntryPoint = "metal_buffer_get_cpu_address")]
        internal static extern MetalResult BufferGetCpuAddress(nint buffer, out nint ptr);

        // ── Heap P/Invoke (P4.1.5) ──
        [DllImport(LibraryName, EntryPoint = "metal_create_heap")]
        internal static extern MetalResult CreateHeap(nint device, ulong size, MetalStorageMode mode, out nint heap);

        [DllImport(LibraryName, EntryPoint = "metal_heap_create_buffer")]
        internal static extern MetalResult HeapCreateBuffer(nint heap, ulong offset, ulong size, out nint buffer);

        // ── Texture P/Invoke (P4.1.3) ──
        [DllImport(LibraryName, EntryPoint = "metal_create_texture")]
        internal static extern MetalResult CreateTexture(
            nint device,
            MetalPixelFormat format,
            uint width,
            uint height,
            uint depth,
            uint levels,
            uint samples,
            MetalTextureType type,
            MetalTextureUsage usageFlags,
            MetalStorageMode storageMode,
            out nint texture);

        [DllImport(LibraryName, EntryPoint = "metal_texture_get_info")]
        internal static extern MetalResult TextureGetInfo(nint texture, out MetalTextureInfo info);

        [DllImport(LibraryName, EntryPoint = "metal_texture_upload")]
        internal static extern MetalResult TextureUpload(
            nint texture,
            nint buffer,
            ulong bufferOffset,
            uint layer,
            uint level,
            uint regionX,
            uint regionY,
            uint regionZ,
            uint regionWidth,
            uint regionHeight,
            uint bytesPerRow);

        [DllImport(LibraryName, EntryPoint = "metal_texture_readback")]
        internal static extern MetalResult TextureReadback(
            nint texture,
            nint buffer,
            ulong bufferOffset,
            uint layer,
            uint level,
            uint bytesPerRow);

        [DllImport(LibraryName, EntryPoint = "metal_create_texture_view")]
        internal static extern MetalResult CreateTextureView(
            nint parentTexture,
            MetalPixelFormat format,
            MetalTextureType type,
            uint firstLayer,
            uint numLayers,
            uint firstLevel,
            uint numLevels,
            out nint outTexture);

        [DllImport(LibraryName, EntryPoint = "metal_pixel_format_get_info")]
        internal static extern MetalPixelFormatInfo PixelFormatGetInfo(MetalPixelFormat format);

        // ── 采样器 P/Invoke (P4.1.4) ──
        [DllImport(LibraryName, EntryPoint = "metal_create_sampler")]
        internal static extern MetalResult CreateSampler(
            nint device,
            in MetalSamplerDescriptor descriptor,
            out nint sampler);
    }

    internal enum MetalResult : uint
    {
        Ok = 0,
        InvalidArgument = 1,
        Unsupported = 2,
        OutOfMemory = 3,
        CompileFailed = 4,
        RuntimeError = 5,
    }

    internal enum MetalStorageMode : uint
    {
        Shared = 0,
        Managed = 1,
        Private = 2,
        Memoryless = 3,
    }

    [Flags]
    internal enum MetalWorkaroundFlags : uint
    {
        None = 0,
        CompilerSingleton = 1u << 0,
        LanguageVersion32 = 1u << 1,
        DiscardGuard = 1u << 2,
        HelperInvocation = 1u << 3,
        SampleMask = 1u << 4,
        TessellationToCompute = 1u << 5,
        TextureFormat = 1u << 6,
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalShaderCompilerConfig
    {
        public uint AbiVersion;
        public MetalWorkaroundFlags EnabledWorkarounds;
        public MetalWorkaroundFlags DisabledWorkarounds;
        public uint MetalLanguageVersion;
        public uint Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalHandleInfo
    {
        public uint AbiVersion;
        public uint Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalBufferInfo
    {
        public ulong Size;
        public MetalStorageMode StorageMode;
        public uint Reserved;
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    internal struct MetalDeviceCaps
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 64)]
        public string DeviceName;
        [MarshalAs(UnmanagedType.U1)]
        public bool HasUnifiedMemory;
        public ulong RegistryId;
        public ulong MaxBufferLength;
        public uint MaxThreadsPerThreadgroupX;
        public uint MaxThreadsPerThreadgroupY;
        public uint MaxThreadsPerThreadgroupZ;
        public uint MaxThreadgroupMemory;
        public uint MaxArgumentBufferSamplerCount;
        [MarshalAs(UnmanagedType.U1)]
        public bool SupportsApple7;
        [MarshalAs(UnmanagedType.U1)]
        public bool SupportsMac1;
        public uint MaxColorAttachments;
        public uint MaxViewports;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)]
        public uint[] Reserved;
    }

    // ── Texture 类型与格式枚举 (P4.1.3) ──

    internal enum MetalTextureType : uint
    {
        Type2D = 0,
        Type2DArray = 1,
        Cube = 2,
        Type3D = 3,
        Type2DMultisample = 4,
    }

    [Flags]
    internal enum MetalTextureUsage : uint
    {
        Unknown = 0,
        ShaderRead = 1u << 0,
        ShaderWrite = 1u << 1,
        RenderTarget = 1u << 2,
        PixelFormatView = 1u << 3,
    }

    internal enum MetalPixelFormat : uint
    {
        Invalid = 0,
        R8Unorm = 1,
        R8Snorm = 2,
        R8Uint = 3,
        R8Sint = 4,
        R16Float = 5,
        R16Unorm = 6,
        R16Uint = 7,
        R16Sint = 8,
        RG8Unorm = 9,
        R32Float = 10,
        R32Uint = 11,
        R32Sint = 12,
        RG16Float = 13,
        RG16Unorm = 14,
        RG16Uint = 15,
        RG16Sint = 16,
        RGBA8Unorm = 17,
        RGBA8Snorm = 18,
        RGBA8Uint = 19,
        RGBA8Sint = 20,
        RGBA8SRgb = 21,
        BGRA8Unorm = 22,
        BGRA8SRgb = 23,
        RG32Float = 24,
        RG32Uint = 25,
        RG32Sint = 26,
        RGBA16Float = 27,
        RGBA16Unorm = 28,
        RGBA16Uint = 29,
        RGBA16Sint = 30,
        RGBA32Float = 31,
        RGBA32Uint = 32,
        RGBA32Sint = 33,
        R10G10B10A2Unorm = 34,
        R11G11B10Float = 35,
        RGB9E5Float = 36,
        D16Unorm = 37,
        D32Float = 38,
        D24UnormS8Uint = 39,
        D32FloatS8Uint = 40,
        BC1RGBA = 41,
        BC2RGBA = 42,
        BC3RGBA = 43,
        BC4R = 44,
        BC5RG = 45,
        BC6HRGB = 46,
        BC7RGBA = 47,
        Astc4x4Ldr = 48,
        Astc6x6Ldr = 49,
        Astc8x8Ldr = 50,
        Astc12x12Ldr = 51,
        Etc2Rgb = 52,
        Etc2Rgba = 53,
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    internal struct MetalPixelFormatInfo
    {
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 32)]
        public string Name;
        public uint BytesPerPixel;
        public uint BlockWidth;
        public uint BlockHeight;
        [MarshalAs(UnmanagedType.U1)]
        public bool IsDepth;
        [MarshalAs(UnmanagedType.U1)]
        public bool IsCompressed;
        [MarshalAs(UnmanagedType.U1)]
        public bool IsSrgb;
        public uint Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalTextureInfo
    {
        public uint Width;
        public uint Height;
        public uint Depth;
        public uint Levels;
        public uint Samples;
        public MetalTextureType Type;
        public MetalPixelFormat PixelFormat;
        public MetalStorageMode StorageMode;
        public uint Reserved;
    }

    // ── 采样器枚举与 P/Invoke (P4.1.4) ──

    internal enum MetalSamplerMinMagFilter : uint
    {
        Nearest = 0,
        Linear = 1,
    }

    internal enum MetalSamplerMipFilter : uint
    {
        NotMipmapped = 0,
        Nearest = 1,
        Linear = 2,
    }

    internal enum MetalSamplerAddressMode : uint
    {
        ClampToEdge = 0,
        Repeat = 1,
        MirroredRepeat = 2,
        ClampToZero = 3,
        ClampToBorderColor = 4,
        MirrorClampToEdge = 5,
    }

    internal enum MetalCompareFunction : uint
    {
        Never = 0,
        Less = 1,
        Equal = 2,
        LessEqual = 3,
        Greater = 4,
        NotEqual = 5,
        GreaterEqual = 6,
        Always = 7,
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalSamplerDescriptor
    {
        public MetalSamplerMinMagFilter MagFilter;
        public MetalSamplerMinMagFilter MinFilter;
        public MetalSamplerMipFilter MipFilter;
        public MetalSamplerAddressMode AddressS;
        public MetalSamplerAddressMode AddressT;
        public MetalSamplerAddressMode AddressR;
        public MetalCompareFunction CompareFunction;
        public float MaxAnisotropy;
        public float LodMinClamp;
        public float LodMaxClamp;
        [MarshalAs(UnmanagedType.U1)]
        public bool NormalizedCoordinates;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 3)]
        public uint[] Reserved;
    }
}
