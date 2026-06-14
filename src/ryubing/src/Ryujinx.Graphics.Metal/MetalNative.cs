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

        // ── Presenter P/Invoke (P4.4.3) ──
        [DllImport(LibraryName, EntryPoint = "metal_create_presenter")]
        internal static extern MetalResult CreatePresenter(
            nint device,
            nint metalLayer,
            out nint presenter);

        [DllImport(LibraryName, EntryPoint = "metal_presenter_get_info")]
        internal static extern MetalResult PresenterGetInfo(
            nint presenter,
            out MetalPresenterInfo info);

        [DllImport(LibraryName, EntryPoint = "metal_presenter_resize")]
        internal static extern MetalResult PresenterResize(
            nint presenter,
            uint drawableWidth,
            uint drawableHeight);

        [DllImport(LibraryName, EntryPoint = "metal_presenter_present_texture")]
        internal static extern MetalResult PresenterPresentTexture(
            nint presenter,
            nint texture);

        // ── 采样器 P/Invoke (P4.1.4) ──
        [DllImport(LibraryName, EntryPoint = "metal_create_sampler")]
        internal static extern MetalResult CreateSampler(
            nint device,
            in MetalSamplerDescriptor descriptor,
            out nint sampler);

        // ── 着色器编译 P/Invoke (P4.2.1) ──
        [DllImport(LibraryName, EntryPoint = "metal_compile_shader")]
        internal static extern MetalShaderCompileResult CompileShader(
            nint compiler,
            string sourceCode,
            string sourceLanguage,
            string stage,
            string entryPoint,
            string profile);

        [DllImport(LibraryName, EntryPoint = "metal_free_shader_data")]
        internal static extern void FreeShaderData(nint data);

        // ── 渲染管线 P/Invoke (P4.3.1) ──
        [DllImport(LibraryName, EntryPoint = "metal_create_render_pipeline")]
        internal static extern MetalResult CreateRenderPipeline(
            nint device,
            in MetalRenderPipelineDescriptor descriptor,
            out nint pipeline);

        [DllImport(LibraryName, EntryPoint = "metal_begin_command_buffer")]
        internal static extern MetalResult BeginCommandBuffer(
            nint queue,
            out nint commandBuffer);

        [DllImport(LibraryName, EntryPoint = "metal_create_shared_event")]
        internal static extern MetalResult CreateSharedEvent(
            nint device,
            out nint sharedEvent);

        [DllImport(LibraryName, EntryPoint = "metal_encode_signal_shared_event")]
        internal static extern MetalResult EncodeSignalSharedEvent(
            nint commandBuffer,
            nint sharedEvent,
            ulong value);

        [DllImport(LibraryName, EntryPoint = "metal_get_shared_event_signaled_value")]
        internal static extern MetalResult GetSharedEventSignaledValue(
            nint sharedEvent,
            out ulong value);

        [DllImport(LibraryName, EntryPoint = "metal_begin_render_encoding_with_targets")]
        internal static extern MetalResult BeginRenderEncodingWithTargets(
            nint commandBuffer,
            nint pipeline,
            [MarshalAs(UnmanagedType.LPArray, SizeConst = 8)] MetalColorAttachmentDescriptor[] colorAttachments,
            uint colorAttachmentCount,
            nint depthStencilPtr,
            out nint renderEncoder);

        // BeginRenderEncodingWithTargets 的便捷重载：不带深度/模板附件
        internal static MetalResult BeginRenderEncodingWithTargets(
            nint commandBuffer,
            nint pipeline,
            MetalColorAttachmentDescriptor[] colorAttachments,
            uint colorAttachmentCount,
            out nint renderEncoder)
        {
            return BeginRenderEncodingWithTargets(
                commandBuffer, pipeline, colorAttachments, colorAttachmentCount,
                nint.Zero, out renderEncoder);
        }

        // BeginRenderEncodingWithTargets 的便捷重载：带深度/模板附件
        internal static MetalResult BeginRenderEncodingWithTargets(
            nint commandBuffer,
            nint pipeline,
            MetalColorAttachmentDescriptor[] colorAttachments,
            uint colorAttachmentCount,
            in MetalDepthStencilAttachmentDescriptor depthStencil,
            out nint renderEncoder)
        {
            // 固定 depthStencil 结构以避免 GC 移动引用类型参数
            GCHandle dsHandle = GCHandle.Alloc(depthStencil, GCHandleType.Pinned);
            try
            {
                return BeginRenderEncodingWithTargets(
                    commandBuffer, pipeline, colorAttachments, colorAttachmentCount,
                    dsHandle.AddrOfPinnedObject(), out renderEncoder);
            }
            finally
            {
                dsHandle.Free();
            }
        }

        [DllImport(LibraryName, EntryPoint = "metal_begin_render_encoding")]
        internal static extern MetalResult BeginRenderEncoding(
            nint commandBuffer,
            nint pipeline,
            out nint renderEncoder);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_vertex_buffer")]
        internal static extern MetalResult RenderEncoderSetVertexBuffer(
            nint renderEncoder,
            uint index,
            nint buffer,
            ulong offset);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_fragment_buffer")]
        internal static extern MetalResult RenderEncoderSetFragmentBuffer(
            nint renderEncoder,
            uint index,
            nint buffer,
            ulong offset);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_fragment_texture")]
        internal static extern MetalResult RenderEncoderSetFragmentTexture(
            nint renderEncoder,
            uint index,
            nint texture);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_fragment_sampler")]
        internal static extern MetalResult RenderEncoderSetFragmentSampler(
            nint renderEncoder,
            uint index,
            nint sampler);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_draw_primitives")]
        internal static extern MetalResult RenderEncoderDrawPrimitives(
            nint renderEncoder,
            MetalPrimitiveType primitiveType,
            uint vertexStart,
            uint vertexCount,
            uint instanceCount,
            uint baseInstance);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_draw_indexed_primitives")]
        internal static extern MetalResult RenderEncoderDrawIndexedPrimitives(
            nint renderEncoder,
            MetalPrimitiveType primitiveType,
            uint indexCount,
            MetalIndexType indexType,
            nint indexBuffer,
            ulong indexBufferOffset,
            uint instanceCount,
            int baseVertex,
            uint baseInstance);

        [DllImport(LibraryName, EntryPoint = "metal_end_render_encoding")]
        internal static extern MetalResult EndRenderEncoding(
            nint renderEncoder);

        [DllImport(LibraryName, EntryPoint = "metal_commit_command_buffer")]
        internal static extern MetalResult CommitCommandBuffer(
            nint commandBuffer);

        [DllImport(LibraryName, EntryPoint = "metal_wait_command_buffer")]
        internal static extern MetalResult WaitCommandBuffer(
            nint commandBuffer);

        // ── 深度/模板状态 P/Invoke (P4.3.10) ──
        [DllImport(LibraryName, EntryPoint = "metal_create_depth_stencil_state")]
        internal static extern MetalResult CreateDepthStencilState(
            nint device,
            in MetalDepthStencilDescriptor descriptor,
            out nint state);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_depth_stencil_state")]
        internal static extern MetalResult RenderEncoderSetDepthStencilState(
            nint renderEncoder,
            nint depthStencilState);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_stencil_reference_value")]
        internal static extern MetalResult RenderEncoderSetStencilReferenceValue(
            nint renderEncoder,
            uint frontValue,
            uint backValue);

        // ── 视口与裁剪矩形 P/Invoke (P4.3.11) ──
        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_viewports")]
        internal static extern MetalResult RenderEncoderSetViewports(
            nint renderEncoder,
            nint viewports,
            uint count);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_scissor_rects")]
        internal static extern MetalResult RenderEncoderSetScissorRects(
            nint renderEncoder,
            nint rects,
            uint count);

        // ── 面剔除与多边形模式 P/Invoke (P4.3.12) ──
        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_cull_mode")]
        internal static extern MetalResult RenderEncoderSetCullMode(
            nint renderEncoder,
            MetalCullMode cullMode);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_front_facing_winding")]
        internal static extern MetalResult RenderEncoderSetFrontFacingWinding(
            nint renderEncoder,
            MetalWinding winding);

        [DllImport(LibraryName, EntryPoint = "metal_render_encoder_set_triangle_fill_mode")]
        internal static extern MetalResult RenderEncoderSetTriangleFillMode(
            nint renderEncoder,
            MetalTriangleFillMode fillMode);
    }

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    internal struct MetalRenderPipelineDescriptor
    {
        public uint AbiVersion;
        public nint VertexMetallibData;
        public ulong VertexMetallibSize;
        public nint FragmentMetallibData;
        public ulong FragmentMetallibSize;
        [MarshalAs(UnmanagedType.LPStr)]
        public string VertexFunction;
        [MarshalAs(UnmanagedType.LPStr)]
        public string FragmentFunction;
        public MetalPixelFormat ColorAttachmentFormat;
        public MetalPixelFormat DepthStencilFormat;
        public uint VertexAttributeCount;
        public uint VertexBufferLayoutCount;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 31)]
        public MetalVertexAttributeDescriptor[] VertexAttributes;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst = 31)]
        public MetalVertexBufferLayoutDescriptor[] VertexBufferLayouts;
        public nint BlendAttachments;
        public uint BlendAttachmentCount;
        public uint Reserved;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalVertexAttributeDescriptor
    {
        public uint AttributeIndex;
        public uint BufferIndex;
        public MetalVertexFormat Format;
        public uint Offset;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalVertexBufferLayoutDescriptor
    {
        public uint BufferIndex;
        public uint Stride;
        public MetalVertexStepFunction StepFunction;
        public uint StepRate;
    }

    internal enum MetalVertexFormat : uint
    {
        Invalid = 0,
        UChar2 = 1,
        UChar3 = 2,
        UChar4 = 3,
        Char2 = 4,
        Char3 = 5,
        Char4 = 6,
        UChar2Normalized = 7,
        UChar3Normalized = 8,
        UChar4Normalized = 9,
        Char2Normalized = 10,
        Char3Normalized = 11,
        Char4Normalized = 12,
        UShort2 = 13,
        UShort3 = 14,
        UShort4 = 15,
        Short2 = 16,
        Short3 = 17,
        Short4 = 18,
        UShort2Normalized = 19,
        UShort3Normalized = 20,
        UShort4Normalized = 21,
        Short2Normalized = 22,
        Short3Normalized = 23,
        Short4Normalized = 24,
        Half2 = 25,
        Half3 = 26,
        Half4 = 27,
        Float = 28,
        Float2 = 29,
        Float3 = 30,
        Float4 = 31,
        Int = 32,
        Int2 = 33,
        Int3 = 34,
        Int4 = 35,
        UInt = 36,
        UInt2 = 37,
        UInt3 = 38,
        UInt4 = 39,
        Int1010102Normalized = 40,
        UInt1010102Normalized = 41,
        UChar4NormalizedBgra = 42,
        UChar = 45,
        Char = 46,
        UCharNormalized = 47,
        CharNormalized = 48,
        UShort = 49,
        Short = 50,
        UShortNormalized = 51,
        ShortNormalized = 52,
        Half = 53,
        FloatRg11B10 = 54,
        FloatRgb9E5 = 55,
    }

    internal enum MetalVertexStepFunction : uint
    {
        Constant = 0,
        PerVertex = 1,
        PerInstance = 2,
    }

    internal enum MetalPrimitiveType : uint
    {
        Point = 0,
        Line = 1,
        LineStrip = 2,
        Triangle = 3,
        TriangleStrip = 4,
    }

    internal enum MetalIndexType : uint
    {
        UInt16 = 0,
        UInt32 = 1,
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

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalPresenterInfo
    {
        public uint AbiVersion;
        public uint DrawableWidth;
        public uint DrawableHeight;
        public MetalPixelFormat PixelFormat;
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

    [StructLayout(LayoutKind.Sequential, CharSet = CharSet.Ansi)]
    internal struct MetalShaderCompileResult
    {
        public MetalResult Result;
        [MarshalAs(UnmanagedType.ByValTStr, SizeConst = 512)]
        public string ErrorMessage;
        private uint _pad0;          ///< 对齐填充，与 C 侧 metal_shader_compile_result._pad0 对应
        public nint MetallibData;
        public ulong MetallibSize;
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

    // ════════════════════════════════════════════════════════════════════
    // 渲染目标附件类型（P4.3.7）
    // ════════════════════════════════════════════════════════════════════

    internal enum MetalLoadAction : uint
    {
        DontCare = 0,
        Load = 1,
        Clear = 2,
    }

    internal enum MetalStoreAction : uint
    {
        DontCare = 0,
        Store = 1,
        MultisampleResolve = 2,
    }

    // ── 面剔除与多边形模式枚举 (P4.3.12) ──
    internal enum MetalCullMode : uint
    {
        None = 0,
        Front = 1,
        Back = 2,
    }

    internal enum MetalWinding : uint
    {
        CounterClockwise = 0,
        Clockwise = 1,
    }

    internal enum MetalTriangleFillMode : uint
    {
        Fill = 0,
        Lines = 1,
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalClearColor
    {
        public double Red;
        public double Green;
        public double Blue;
        public double Alpha;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalClearDepthStencil
    {
        public double Depth;
        public uint Stencil;
        public uint Reserved;
    }

    // ══════════════════════════════════════════════════════════════════
    // 混合状态类型（P4.3.9）
    // ══════════════════════════════════════════════════════════════════

    internal enum MetalBlendFactor : uint
    {
        Zero = 0,
        One = 1,
        SrcColor = 2,
        OneMinusSrcColor = 3,
        SrcAlpha = 4,
        OneMinusSrcAlpha = 5,
        DstAlpha = 6,
        OneMinusDstAlpha = 7,
        DstColor = 8,
        OneMinusDstColor = 9,
        SrcAlphaSaturate = 10,
        BlendColor = 11,
        OneMinusBlendColor = 12,
        BlendAlpha = 13,
        OneMinusBlendAlpha = 14,
        Src1Color = 15,
        OneMinusSrc1Color = 16,
        Src1Alpha = 17,
        OneMinusSrc1Alpha = 18,
    }

    internal enum MetalBlendOperation : uint
    {
        Add = 0,
        Subtract = 1,
        ReverseSubtract = 2,
        Min = 3,
        Max = 4,
    }

    [Flags]
    internal enum MetalColorWriteMask : uint
    {
        None = 0,
        Red = 1u << 3,
        Green = 1u << 2,
        Blue = 1u << 1,
        Alpha = 1u << 0,
        All = Red | Green | Blue | Alpha,
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalBlendAttachmentDescriptor
    {
        public byte BlendingEnabled;
        public byte ReservedPad0;
        public byte ReservedPad1;
        public byte ReservedPad2;
        public MetalBlendFactor SrcRgbFactor;
        public MetalBlendFactor DstRgbFactor;
        public MetalBlendOperation RgbOperation;
        public MetalBlendFactor SrcAlphaFactor;
        public MetalBlendFactor DstAlphaFactor;
        public MetalBlendOperation AlphaOperation;
        public uint WriteMask;
    }

    // ══════════════════════════════════════════════════════════════════
    // 深度/模板状态类型（P4.3.10）
    // ══════════════════════════════════════════════════════════════════

    internal enum MetalStencilOperation : uint
    {
        Keep = 0,
        Zero = 1,
        Replace = 2,
        IncrementClamp = 3,
        DecrementClamp = 4,
        Invert = 5,
        IncrementWrap = 6,
        DecrementWrap = 7,
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalStencilDescriptor
    {
        public MetalCompareFunction CompareFunction;
        public MetalStencilOperation StencilFailure;
        public MetalStencilOperation DepthFailure;
        public MetalStencilOperation DepthStencilPass;
        public uint ReadMask;
        public uint WriteMask;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalDepthStencilDescriptor
    {
        public MetalCompareFunction DepthCompareFunction;
        public byte DepthWriteEnabled;
        public byte StencilEnabled;
        public byte ReservedPad0;
        public byte ReservedPad1;
        public MetalStencilDescriptor FrontFace;
        public MetalStencilDescriptor BackFace;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalColorAttachmentDescriptor
    {
        public nint Texture;
        public uint Level;
        public uint Slice;
        public MetalLoadAction LoadAction;
        public MetalStoreAction StoreAction;
        public MetalClearColor ClearColor;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalDepthStencilAttachmentDescriptor
    {
        public nint Texture;
        public uint Level;
        public uint Slice;
        public MetalLoadAction DepthLoadAction;
        public MetalStoreAction DepthStoreAction;
        public MetalLoadAction StencilLoadAction;
        public MetalStoreAction StencilStoreAction;
        public MetalClearDepthStencil ClearValue;
    }

    // ── 视口与裁剪矩形描述符 (P4.3.11) ──
    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalViewport
    {
        public double OriginX;
        public double OriginY;
        public double Width;
        public double Height;
        public double ZNear;
        public double ZFar;
    }

    [StructLayout(LayoutKind.Sequential)]
    internal struct MetalScissorRect
    {
        public uint X;
        public uint Y;
        public uint Width;
        public uint Height;
    }
}
