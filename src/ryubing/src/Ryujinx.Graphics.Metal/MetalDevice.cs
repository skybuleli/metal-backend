using System;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    internal sealed class MetalDevice : IDisposable
    {
        private nint _handle;
        private MetalHandleInfo _handleInfo;
        private MetalDeviceCaps _caps;

        public nint Handle => _handle;

        public uint AbiVersion => _handleInfo.AbiVersion;

        public ref readonly MetalDeviceCaps Caps => ref _caps;

        public bool HasUnifiedMemory => _caps.HasUnifiedMemory;

        private MetalDevice(nint handle, MetalHandleInfo handleInfo, MetalDeviceCaps caps)
        {
            _handle = handle;
            _handleInfo = handleInfo;
            _caps = caps;
        }

        public static MetalDevice Create()
        {
            uint abiVersion = MetalNative.BridgeAbiVersion();

            if (abiVersion != MetalNative.AbiVersion)
            {
                throw new InvalidOperationException($"libmetal_bridge ABI 不匹配：native={abiVersion} managed={MetalNative.AbiVersion}。");
            }

            MetalResult result = MetalNative.CreateDevice(out nint device);

            if (result != MetalResult.Ok)
            {
                throw CreateException(nameof(MetalNative.CreateDevice), result);
            }

            result = MetalNative.GetDeviceInfo(device, out MetalHandleInfo info);

            if (result != MetalResult.Ok)
            {
                MetalNative.Release(device);
                throw CreateException(nameof(MetalNative.GetDeviceInfo), result);
            }

            MetalResult capsResult = MetalNative.GetDeviceCaps(device, out MetalDeviceCaps caps);

            if (capsResult != MetalResult.Ok)
            {
                MetalNative.Release(device);
                throw CreateException(nameof(MetalNative.GetDeviceCaps), capsResult);
            }

            return new MetalDevice(device, info, caps);
        }

        public void Dispose()
        {
            if (_handle != nint.Zero)
            {
                MetalNative.Release(_handle);
                _handle = nint.Zero;
            }
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
}
