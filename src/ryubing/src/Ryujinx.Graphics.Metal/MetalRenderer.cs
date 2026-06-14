using System.Threading;
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
        private readonly MetalSync _sync;
        private readonly bool _hasUnifiedMemory;
        private readonly MetalStorageMode _defaultStorageMode;
        private readonly nint _queueHandle;
        private readonly nint _backgroundQueueHandle;
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

            MetalResult queueResult = MetalNative.CreateQueue(_device.Handle, out nint queueHandle);
            if (queueResult != MetalResult.Ok)
            {
                throw new InvalidOperationException($"CreateQueue 失败：{queueResult}");
            }

            _queueHandle = queueHandle;

            // 后台队列：用于 BackgroundContextAction 等独立 GPU 提交路径
            MetalResult bgQueueResult = MetalNative.CreateQueue(_device.Handle, out nint bgQueueHandle);
            if (bgQueueResult != MetalResult.Ok)
            {
                MetalNative.Release(queueHandle);
                throw new InvalidOperationException($"CreateQueue（后台）失败：{bgQueueResult}");
            }

            _backgroundQueueHandle = bgQueueHandle;
            _buffers = new MetalBufferPool(_device.Handle, _defaultStorageMode);
            _shaderCompiler = new MetalShaderCompiler();
            _shaderCompiler.AttachDevice(_device); // 绑定设备，初始化编译器句柄
            _pipeline = new MetalPipeline(_device.Handle, _queueHandle, _buffers);
            _sync = new MetalSync(_device.Handle, _queueHandle);
            _nullTextureArray = new MetalTextureArray(0, false);
            _nullImageArray = new MetalImageArray(0, false);
            _window = new MetalWindow(_device.Handle);
            _window.ScreenCapturedCallback = info => ScreenCaptured?.Invoke(this, info);

        }

        public IWindow Window => _window;

        /// <summary>
        /// 设置 CAMetalLayer 指针（由 AppHost 层传入）。
        /// </summary>
        public void SetLayer(nint metalLayer)
        {
            _window.SetLayer(metalLayer);
        }

        public void RunLoop(ThreadStart gpuLoop)
        {
            gpuLoop();
        }

        public uint ProgramCount => _programCount;
        public void BackgroundContextAction(Action action, bool alwaysBackground = false)
        {
            if (alwaysBackground)
            {
                // 确保在后台线程执行，避免阻塞渲染线程
                System.Threading.ThreadPool.QueueUserWorkItem(_ => action());
            }
            else
            {
                // 当前上下文直接执行（调用者期望同步）
                action();
            }
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
            _sync.Create(id, strict);
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
            if (_backgroundQueueHandle != nint.Zero)
            {
                MetalNative.Release(_backgroundQueueHandle);
            }
            _nullTextureArray.Dispose();
            _sync.Dispose();
            _shaderCompiler.Dispose();
            if (_queueHandle != nint.Zero)
            {
                MetalNative.Release(_queueHandle);
            }
            _device.Dispose();
        }

        public PinnedSpan<byte> GetBufferData(BufferHandle buffer, int offset, int size)
        {
            return _buffers.GetData(buffer, offset, size);
        }

        public Capabilities GetCapabilities()
        {
            var memoryType = _device.Caps.HasUnifiedMemory
                ? SystemMemoryType.UnifiedMemory
                : SystemMemoryType.DedicatedMemory;

            return new Capabilities(
                api: TargetApi.Metal,
                vendorName: _device.Caps.DeviceName,
                memoryType: memoryType,
                hasFrontFacingBug: false,
                hasVectorIndexingBug: false,
                needsFragmentOutputSpecialization: false,
                reduceShaderPrecision: false,
                supportsAstcCompression: true,
                supportsBc123Compression: true,
                supportsBc45Compression: true,
                supportsBc67Compression: true,
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
                maximumComputeSharedMemorySize: (int)_device.Caps.MaxThreadgroupMemory,
                maximumSupportedAnisotropy: 16f,
                shaderSubgroupSize: 32,
                storageBufferOffsetAlignment: 16,
                textureBufferOffsetAlignment: 16,
                gatherBiasPrecision: 0,
                maximumGpuMemory: 0);
        }

        public ulong GetCurrentSync()
        {
            return _sync.GetCurrent();
        }

        public HardwareInfo GetHardwareInfo()
        {
            return new HardwareInfo("Apple", _device.Caps.DeviceName, "libmetal_bridge");
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
            _sync.Cleanup();
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
            _window.ScreenCaptureRequested = true;
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
            _sync.Wait(id);
        }
    }
}
