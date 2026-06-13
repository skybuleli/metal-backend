using Ryujinx.Common.Configuration;
using Ryujinx.Graphics.GAL;
using Ryujinx.Graphics.Shader.Translation;
using System;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    public sealed class MetalRenderer : IRenderer
    {
        private readonly MetalShaderCompiler _shaderCompiler;
        private readonly MetalPipeline _pipeline;
        private Action<Action> _interruptAction;
        private uint _programCount;

        public event EventHandler<ScreenCaptureImageInfo> ScreenCaptured;

        public bool PreferThreading => false;

        public IPipeline Pipeline => _pipeline;

        public MetalRenderer()
        {
            _shaderCompiler = new MetalShaderCompiler();
            _pipeline = new MetalPipeline();
        }

        public IWindow Window => throw new NotSupportedException("P3.7 之后再接入 Metal 窗口与 presenter。");

        public uint ProgramCount => _programCount;

        public void BackgroundContextAction(Action action, bool alwaysBackground = false)
        {
            action();
        }

        public BufferHandle CreateBuffer(int size, BufferAccess access = BufferAccess.Default)
        {
            throw new NotSupportedException();
        }

        public BufferHandle CreateBuffer(nint pointer, int size)
        {
            throw new NotSupportedException();
        }

        public BufferHandle CreateBufferSparse(ReadOnlySpan<BufferRange> storageBuffers)
        {
            throw new NotSupportedException();
        }

        public IImageArray CreateImageArray(int size, bool isBuffer)
        {
            throw new NotSupportedException();
        }

        public IProgram CreateProgram(ShaderSource[] shaders, ShaderInfo info)
        {
            _programCount++;
            return _shaderCompiler.CreateProgram(shaders, info);
        }

        public ISampler CreateSampler(SamplerCreateInfo info)
        {
            throw new NotSupportedException();
        }

        public ITexture CreateTexture(TextureCreateInfo info)
        {
            throw new NotSupportedException();
        }

        public ITextureArray CreateTextureArray(int size, bool isBuffer)
        {
            throw new NotSupportedException();
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
            throw new NotSupportedException();
        }

        public void Dispose()
        {
            _shaderCompiler.Dispose();
        }

        public PinnedSpan<byte> GetBufferData(BufferHandle buffer, int offset, int size)
        {
            throw new NotSupportedException();
        }

        public Capabilities GetCapabilities()
        {
            throw new NotSupportedException();
        }

        public ulong GetCurrentSync()
        {
            return 0;
        }

        public HardwareInfo GetHardwareInfo()
        {
            throw new NotSupportedException();
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
            throw new NotSupportedException();
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
