using Ryujinx.Common.Configuration;
using Ryujinx.Common.Memory;
using Ryujinx.Graphics.GAL;
using Ryujinx.Graphics.Shader.Translation;
using System;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    public sealed class MetalRenderer : IRenderer
    {
        private readonly MetalBufferPool _buffers;
        private readonly MetalShaderCompiler _shaderCompiler;
        private readonly MetalImageArray _nullImageArray;
        private readonly MetalPipeline _pipeline;
        private readonly MetalTextureArray _nullTextureArray;
        private readonly MetalWindow _window;
        private readonly MetalDevice _device;
        private readonly bool _hasUnifiedMemory;
        private readonly MetalStorageMode _defaultStorageMode;
        private Action<Action> _interruptAction;
        private uint _programCount;

        public event EventHandler<ScreenCaptureImageInfo> ScreenCaptured;

        public bool PreferThreading => false;

        public IPipeline Pipeline => _pipeline;

        public MetalRenderer()
        {
            _device = MetalDevice.Create();
            _hasUnifiedMemory = _device.HasUnifiedMemory;

            // UMA（Apple Silicon）使用 Shared 存储模式，避免 map/unmap 开销
            // 离散 GPU（Intel Mac）使用 Managed 模式，需要显式 flush
            _defaultStorageMode = _hasUnifiedMemory
                ? MetalStorageMode.Shared
                : MetalStorageMode.Managed;

            _buffers = new MetalBufferPool(_device.Handle, _defaultStorageMode);
            _shaderCompiler = new MetalShaderCompiler();
            _pipeline = new MetalPipeline();
            _nullTextureArray = new MetalTextureArray(0, false);
            _nullImageArray = new MetalImageArray(0, false);
            _window = new MetalWindow();
        }

        public IWindow Window => _window;

        public uint ProgramCount => _programCount;

        public void BackgroundContextAction(Action action, bool alwaysBackground = false)
        {
            action();
        }

        public BufferHandle CreateBuffer(int size, BufferAccess access = BufferAccess.Default)
        {
            return _buffers.Create(size, access);
        }

        public BufferHandle CreateBuffer(nint pointer, int size)
        {
            return _buffers.CreateImported(pointer, size);
        }

        public BufferHandle CreateBufferSparse(ReadOnlySpan<BufferRange> storageBuffers)
        {
            return _buffers.CreateSparse(storageBuffers);
        }

        public IImageArray CreateImageArray(int size, bool isBuffer)
        {
            return new MetalImageArray(size, isBuffer);
        }

        public IProgram CreateProgram(ShaderSource[] shaders, ShaderInfo info)
        {
            _programCount++;
            return _shaderCompiler.CreateProgram(shaders, info);
        }

        public ISampler CreateSampler(SamplerCreateInfo info)
        {
            return new MetalSampler(_device.Handle, info);
        }

        public ITexture CreateTexture(TextureCreateInfo info)
        {
            return new MetalTexture(_device.Handle, info, _defaultStorageMode);
        }

        public ITextureArray CreateTextureArray(int size, bool isBuffer)
        {
            return new MetalTextureArray(size, isBuffer);
        }

        public bool PrepareHostMapping(nint address, ulong size)
        {
            return false;
        }

        public void CreateSync(ulong id, bool strict)
        {
        }

        public void DeleteBuffer(BufferHandle buffer)
        {
            _buffers.Delete(buffer);
        }

        public void Dispose()
        {
            _buffers.Dispose();
            _window.Dispose();
            _nullImageArray.Dispose();
            _nullTextureArray.Dispose();
            _shaderCompiler.Dispose();
            _device.Dispose();
        }

        public PinnedSpan<byte> GetBufferData(BufferHandle buffer, int offset, int size)
        {
            return _buffers.GetData(buffer, offset, size);
        }

        public Capabilities GetCapabilities()
        {
            return new Capabilities(
                api: TargetApi.Metal,
                vendorName: "Apple Metal",
                memoryType: SystemMemoryType.UnifiedMemory,
                hasFrontFacingBug: false,
                hasVectorIndexingBug: false,
                needsFragmentOutputSpecialization: false,
                reduceShaderPrecision: false,
                supportsAstcCompression: true,
                supportsBc123Compression: false,
                supportsBc45Compression: false,
                supportsBc67Compression: false,
                supportsEtc2Compression: true,
                supports3DTextureCompression: true,
                supportsBgraFormat: true,
                supportsR4G4Format: false,
                supportsR4G4B4A4Format: true,
                supportsScaledVertexFormats: true,
                supportsSnormBufferTextureFormat: true,
                supports5BitComponentFormat: true,
                supportsSparseBuffer: false,
                supportsBlendEquationAdvanced: false,
                supportsFragmentShaderInterlock: false,
                supportsFragmentShaderOrderingIntel: false,
                supportsGeometryShader: false,
                supportsGeometryShaderPassthrough: false,
                supportsTransformFeedback: false,
                supportsImageLoadFormatted: true,
                supportsLayerVertexTessellation: true,
                supportsMismatchingViewFormat: true,
                supportsCubemapView: true,
                supportsNonConstantTextureOffset: true,
                supportsQuads: false,
                supportsSeparateSampler: true,
                supportsShaderBallot: false,
                supportsShaderBarrierDivergence: true,
                supportsShaderFloat64: false,
                supportsShaderNonUniformIndexing: true,
                supportsTextureGatherOffsets: true,
                supportsTextureShadowLod: true,
                supportsVertexStoreAndAtomics: true,
                supportsViewportIndexVertexTessellation: true,
                supportsViewportMask: false,
                supportsViewportSwizzle: false,
                supportsIndirectParameters: true,
                supportsDepthClipControl: true,
                uniformBufferSetIndex: 0,
                storageBufferSetIndex: 1,
                textureSetIndex: 2,
                imageSetIndex: 3,
                extraSetBaseIndex: 0,
                maximumExtraSets: 0,
                maximumUniformBuffersPerStage: 16,
                maximumStorageBuffersPerStage: 16,
                maximumTexturesPerStage: 32,
                maximumImagesPerStage: 16,
                maximumComputeSharedMemorySize: 32768,
                maximumSupportedAnisotropy: 16f,
                shaderSubgroupSize: 32,
                storageBufferOffsetAlignment: 16,
                textureBufferOffsetAlignment: 16,
                gatherBiasPrecision: 0,
                maximumGpuMemory: 0);
        }

        public ulong GetCurrentSync()
        {
            return 0;
        }

        public HardwareInfo GetHardwareInfo()
        {
            return new HardwareInfo("Apple", "Metal Stub Renderer", "libmetal_bridge");
        }

        public void Initialize(GraphicsDebugLevel logLevel)
        {
        }

        public IProgram LoadProgramBinary(byte[] programBinary, bool hasFragmentShader, ShaderInfo info)
        {
            _programCount++;
            return _shaderCompiler.LoadProgramBinary(programBinary, hasFragmentShader, info);
        }

        public void PreFrame()
        {
        }

        public ICounterEvent ReportCounter(CounterType type, EventHandler<ulong> resultHandler, float divisor, bool hostReserved)
        {
            throw new NotSupportedException();
        }

        public void ResetCounter(CounterType type)
        {
        }

        public void Screenshot()
        {
            ScreenCaptured?.Invoke(this, default);
        }

        public void SetBufferData(BufferHandle buffer, int offset, ReadOnlySpan<byte> data)
        {
            _buffers.SetData(buffer, offset, data);
        }

        public void SetInterruptAction(Action<Action> interruptAction)
        {
            _interruptAction = interruptAction;
        }

        public void UpdateCounters()
        {
        }

        public void WaitSync(ulong id)
        {
        }
    }
}
