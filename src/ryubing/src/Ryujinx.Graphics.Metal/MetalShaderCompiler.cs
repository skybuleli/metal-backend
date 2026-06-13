using Ryujinx.Graphics.GAL;
using System;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    internal sealed class MetalShaderCompiler : IDisposable
    {
        private MetalDevice _device;
        private nint _compilerHandle;
        private MetalShaderCompilerConfig _config;
        private MetalWorkaroundFlags _activeWorkarounds;

        public MetalWorkaroundFlags ActiveWorkarounds => _activeWorkarounds;

        public void AttachDevice(MetalDevice device)
        {
            ArgumentNullException.ThrowIfNull(device);

            if (ReferenceEquals(_device, device) && _compilerHandle != nint.Zero)
            {
                return;
            }

            ReleaseCompiler();

            _device = device;

            MetalResult result = MetalNative.AcquireShaderCompiler(device.Handle, out nint compilerHandle);

            if (result != MetalResult.Ok)
            {
                throw CreateException(nameof(MetalNative.AcquireShaderCompiler), result);
            }

            result = MetalNative.GetDefaultShaderCompilerConfig(out MetalShaderCompilerConfig config);

            if (result != MetalResult.Ok)
            {
                MetalNative.Release(compilerHandle);
                throw CreateException(nameof(MetalNative.GetDefaultShaderCompilerConfig), result);
            }

            config.AbiVersion = MetalNative.AbiVersion;

            result = MetalNative.ConfigureShaderCompiler(compilerHandle, config);

            if (result != MetalResult.Ok)
            {
                MetalNative.Release(compilerHandle);
                throw CreateException(nameof(MetalNative.ConfigureShaderCompiler), result);
            }

            _compilerHandle = compilerHandle;
            _config = config;
            _activeWorkarounds = (MetalWorkaroundFlags)MetalNative.ShaderCompilerGetWorkarounds(compilerHandle);
        }

        public IProgram CreateProgram(ShaderSource[] shaders, ShaderInfo info)
        {
            ArgumentNullException.ThrowIfNull(shaders);

            return new MetalProgram(info, _activeWorkarounds);
        }

        public IProgram LoadProgramBinary(byte[] programBinary, bool hasFragmentShader, ShaderInfo info)
        {
            ArgumentNullException.ThrowIfNull(programBinary);

            return new MetalProgram(programBinary, hasFragmentShader, info, _activeWorkarounds);
        }

        public void Dispose()
        {
            ReleaseCompiler();
        }

        private void ReleaseCompiler()
        {
            if (_compilerHandle != nint.Zero)
            {
                MetalNative.Release(_compilerHandle);
                _compilerHandle = nint.Zero;
            }

            _activeWorkarounds = default;
            _config = default;
            _device = null;
        }

        private static InvalidOperationException CreateException(string operation, MetalResult result)
        {
            nint errorMessagePtr = MetalNative.GetLastErrorMessage();
            string errorMessage = errorMessagePtr != nint.Zero
                ? Marshal.PtrToStringAnsi(errorMessagePtr) ?? "未知错误。"
                : "未提供错误消息。";

            return new InvalidOperationException($"{operation} 失败：{result}，{errorMessage}");
        }
    }

    internal sealed class MetalProgram : IProgram
    {
        private readonly byte[] _binary;

        public ProgramLinkStatus LinkStatus { get; private set; }

        public MetalWorkaroundFlags ActiveWorkarounds { get; }

        public int FragmentOutputMap { get; }

        public bool HasFragmentShader { get; }

        public MetalProgram(ShaderInfo info, MetalWorkaroundFlags activeWorkarounds)
        {
            FragmentOutputMap = info.FragmentOutputMap;
            ActiveWorkarounds = activeWorkarounds;
            LinkStatus = ProgramLinkStatus.Success;
        }

        public MetalProgram(byte[] binary, bool hasFragmentShader, ShaderInfo info, MetalWorkaroundFlags activeWorkarounds)
        {
            _binary = binary;
            FragmentOutputMap = info.FragmentOutputMap;
            HasFragmentShader = hasFragmentShader;
            ActiveWorkarounds = activeWorkarounds;
            LinkStatus = ProgramLinkStatus.Success;
        }

        public ProgramLinkStatus CheckProgramLink(bool blocking)
        {
            return LinkStatus;
        }

        public byte[] GetBinary()
        {
            return _binary ?? Array.Empty<byte>();
        }

        public void Dispose()
        {
        }
    }
}
