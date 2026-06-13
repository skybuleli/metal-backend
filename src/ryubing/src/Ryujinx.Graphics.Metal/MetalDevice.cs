using System;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    internal sealed class MetalDevice : IDisposable
    {
        private nint _handle;
        private readonly MetalHandleInfo _handleInfo;

        public nint Handle => _handle;

        public uint AbiVersion => _handleInfo.AbiVersion;

        private MetalDevice(nint handle, MetalHandleInfo handleInfo)
        {
            _handle = handle;
            _handleInfo = handleInfo;
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

            return new MetalDevice(device, info);
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
