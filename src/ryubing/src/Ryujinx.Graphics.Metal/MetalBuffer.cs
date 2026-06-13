using System;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    internal sealed class MetalBuffer : IDisposable
    {
        private nint _handle;
        private readonly MetalBufferInfo _info;
        private bool _disposed;

        public nint Handle => _handle;

        public ulong Size => _info.Size;

        public MetalStorageMode StorageMode => _info.StorageMode;

        public MetalBuffer(nint handle, MetalBufferInfo info)
        {
            _handle = handle;
            _info = info;
        }

        /// <summary>
        /// 将指定范围内的数据刷新到 GPU（Managed 模式下必需）。
        /// </summary>
        public MetalResult Flush(ulong offset, ulong size)
        {
            return MetalNative.FlushBuffer(_handle, offset, size);
        }

        public void Dispose()
        {
            if (!_disposed && _handle != nint.Zero)
            {
                MetalNative.Release(_handle);
                _handle = nint.Zero;
                _disposed = true;
            }
        }
    }
}
