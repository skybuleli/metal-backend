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

        [LibraryImport(LibraryName, EntryPoint = "metal_bridge_abi_version")]
        internal static partial uint BridgeAbiVersion();

        [LibraryImport(LibraryName, EntryPoint = "metal_release")]
        internal static partial void Release(nint handle);

        [LibraryImport(LibraryName, EntryPoint = "metal_get_last_error_message")]
        internal static partial nint GetLastErrorMessage();

        [LibraryImport(LibraryName, EntryPoint = "metal_create_device")]
        internal static partial MetalResult CreateDevice(out nint device);

        [LibraryImport(LibraryName, EntryPoint = "metal_get_device_info")]
        internal static partial MetalResult GetDeviceInfo(nint device, out MetalHandleInfo info);

        [LibraryImport(LibraryName, EntryPoint = "metal_create_queue")]
        internal static partial MetalResult CreateQueue(nint device, out nint queue);

        [LibraryImport(LibraryName, EntryPoint = "metal_acquire_shader_compiler")]
        internal static partial MetalResult AcquireShaderCompiler(nint device, out nint compiler);

        [LibraryImport(LibraryName, EntryPoint = "metal_get_default_shader_compiler_config")]
        internal static partial MetalResult GetDefaultShaderCompilerConfig(out MetalShaderCompilerConfig config);

        [LibraryImport(LibraryName, EntryPoint = "metal_configure_shader_compiler")]
        internal static partial MetalResult ConfigureShaderCompiler(nint compiler, in MetalShaderCompilerConfig config);

        [LibraryImport(LibraryName, EntryPoint = "metal_shader_compiler_get_workarounds")]
        internal static partial uint ShaderCompilerGetWorkarounds(nint compiler);

        // ── Buffer P/Invoke (P4.1.2) ──
        [LibraryImport(LibraryName, EntryPoint = "metal_create_buffer")]
        internal static partial MetalResult CreateBuffer(nint device, ulong size, MetalStorageMode mode, out nint buffer);

        [LibraryImport(LibraryName, EntryPoint = "metal_create_buffer_with_bytes")]
        internal static partial MetalResult CreateBufferWithBytes(nint device, nint data, ulong size, MetalStorageMode mode, out nint buffer);

        [LibraryImport(LibraryName, EntryPoint = "metal_buffer_get_info")]
        internal static partial MetalResult BufferGetInfo(nint buffer, out MetalBufferInfo info);

        [LibraryImport(LibraryName, EntryPoint = "metal_map_buffer")]
        internal static partial MetalResult MapBuffer(nint buffer, out nint ptr);

        [LibraryImport(LibraryName, EntryPoint = "metal_unmap_buffer")]
        internal static partial MetalResult UnmapBuffer(nint buffer);

        [LibraryImport(LibraryName, EntryPoint = "metal_flush_buffer")]
        internal static partial MetalResult FlushBuffer(nint buffer, ulong offset, ulong size);
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
}
