using Ryujinx.Common.Memory;
using Ryujinx.Common.Logging;
using Ryujinx.Graphics.GAL;
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;

namespace Ryujinx.Graphics.Metal
{
    internal sealed class MetalBufferPool : IDisposable
    {
        private readonly nint _deviceHandle;
        private readonly MetalStorageMode _defaultMode;
        private readonly Dictionary<int, MetalBuffer> _buffers = [];
        private readonly List<nint> _heapHandles = []; // P4.1.5: 堆句柄跟踪
        private int _nextHandle = 1;

        public MetalBufferPool(nint deviceHandle, MetalStorageMode defaultMode)
        {
            _deviceHandle = deviceHandle;
            _defaultMode = defaultMode;
        }

        public BufferHandle Create(int size, BufferAccess access)
        {
            int actualSize = Math.Max(size, 1);

            MetalResult result = MetalNative.CreateBuffer(
                _deviceHandle, (ulong)actualSize, _defaultMode, out nint bufHandle);

            if (result != MetalResult.Ok)
            {
                // 回退：创建最小缓冲区
                result = MetalNative.CreateBuffer(_deviceHandle, 16, _defaultMode, out bufHandle);
                if (result != MetalResult.Ok)
                {
                    return default;
                }
            }

            result = MetalNative.BufferGetInfo(bufHandle, out MetalBufferInfo info);
            if (result != MetalResult.Ok)
            {
                MetalNative.Release(bufHandle);
                return default;
            }

            var metalBuf = new MetalBuffer(bufHandle, info);
            int handleValue = _nextHandle++;
            _buffers.Add(handleValue, metalBuf);

            ulong rawHandle = (uint)handleValue;
            return Unsafe.As<ulong, BufferHandle>(ref rawHandle);
        }

        public BufferHandle CreateImported(nint pointer, int size)
        {
            int actualSize = Math.Max(size, 1);

            // 零拷贝包装：传入的指针直接在 Metal 中作为 Shared 缓冲区使用
            MetalResult result = MetalNative.CreateBufferFromPointer(
                _deviceHandle, pointer, (ulong)actualSize, MetalStorageMode.Shared, out nint bufHandle);

            if (result != MetalResult.Ok)
            {
                return default;
            }

            result = MetalNative.BufferGetInfo(bufHandle, out MetalBufferInfo info);
            if (result != MetalResult.Ok)
            {
                MetalNative.Release(bufHandle);
                return default;
            }

            var metalBuf = new MetalBuffer(bufHandle, info);
            int handleValue = _nextHandle++;
            _buffers.Add(handleValue, metalBuf);

            ulong rawHandle = (uint)handleValue;
            return Unsafe.As<ulong, BufferHandle>(ref rawHandle);
        }

        public BufferHandle CreateSparse(ReadOnlySpan<BufferRange> storageBuffers)
        {
            // P4.1.5: 使用 MTLHeap 实现稀疏缓冲区
            // MTLHeap 允许多个 MTLBuffer 共享同一块物理内存，
            // 通过 offset+size 创建不同区域的视图。
            //
            // 当前实现：计算总大小 → 创建 MTLHeap → 分配总缓冲区
            // 后续可扩展为多个子缓冲区视图（offset/range 级映射）
            long totalSize = 0;

            foreach (BufferRange range in storageBuffers)
            {
                totalSize += Math.Max(range.Size, 0);
            }

            if (totalSize == 0)
            {
                return Create(1, BufferAccess.Default);
            }

            // 创建 MTLHeap 作为稀疏缓冲区的物理存储后端
            // Private 模式：GPU 专用，CPU 不可直接访问，最优性能
            MetalResult result = MetalNative.CreateHeap(
                _deviceHandle, (ulong)totalSize, _defaultMode, out nint heapHandle);

            if (result != MetalResult.Ok)
            {
                // 回退：heap 创建失败则使用普通缓冲区
                return Create((int)totalSize, BufferAccess.Default);
            }

            _heapHandles.Add(heapHandle);

            // 从堆分配总缓冲区（offset=0，覆盖整个堆）
            result = MetalNative.HeapCreateBuffer(
                heapHandle, 0, (ulong)totalSize, out nint bufHandle);

            if (result != MetalResult.Ok)
            {
                MetalNative.Release(heapHandle);
                _heapHandles.Remove(heapHandle);
                return Create((int)totalSize, BufferAccess.Default);
            }

            result = MetalNative.BufferGetInfo(bufHandle, out MetalBufferInfo info);
            if (result != MetalResult.Ok)
            {
                MetalNative.Release(bufHandle);
                MetalNative.Release(heapHandle);
                _heapHandles.Remove(heapHandle);
                return Create((int)totalSize, BufferAccess.Default);
            }

            var metalBuf = new MetalBuffer(bufHandle, info);
            int handleValue = _nextHandle++;
            _buffers.Add(handleValue, metalBuf);

            ulong rawHandle = (uint)handleValue;
            return Unsafe.As<ulong, BufferHandle>(ref rawHandle);
        }

        public void Delete(BufferHandle buffer)
        {
            if (_buffers.Remove((int)buffer, out MetalBuffer metalBuf))
            {
                metalBuf.Dispose();
            }
        }

        internal bool TryGet(BufferHandle buffer, out MetalBuffer metalBuffer)
        {
            return _buffers.TryGetValue((int)buffer, out metalBuffer);
        }

        public PinnedSpan<byte> GetData(BufferHandle buffer, int offset, int size)
        {
            if (!_buffers.TryGetValue((int)buffer, out MetalBuffer metalBuf))
            {
                return default;
            }

            int safeOffset = Math.Clamp(offset, 0, (int)metalBuf.Size);
            int safeLength = Math.Clamp(size, 0, (int)metalBuf.Size - safeOffset);

            if (safeLength <= 0)
            {
                return default;
            }

            MetalResult result = MetalNative.MapBuffer(metalBuf.Handle, out nint ptr);
            if (result != MetalResult.Ok)
            {
                return default;
            }

            try
            {
                byte[] data = new byte[safeLength];
                Marshal.Copy(ptr + safeOffset, data, 0, safeLength);
                return PinnedSpan<byte>.UnsafeFromSpan(data.AsSpan(0, safeLength));
            }
            finally
            {
                MetalNative.UnmapBuffer(metalBuf.Handle);
            }
        }

        public void SetData(BufferHandle buffer, int offset, ReadOnlySpan<byte> data)
        {
            if (!_buffers.TryGetValue((int)buffer, out MetalBuffer metalBuf))
            {
                return;
            }

            int safeOffset = Math.Clamp(offset, 0, (int)metalBuf.Size);
            int copyLength = Math.Min(data.Length, (int)metalBuf.Size - safeOffset);

            if (copyLength <= 0)
            {
                return;
            }

            MetalResult result = MetalNative.MapBuffer(metalBuf.Handle, out nint ptr);
            if (result != MetalResult.Ok)
            {
                return;
            }

            try
            {
                unsafe
                {
                    fixed (byte* srcPtr = data)
                    {
                        System.Buffer.MemoryCopy(
                            srcPtr,
                            (void*)(ptr + safeOffset),
                            copyLength,
                            copyLength);
                    }
                }
            }
            finally
            {
                MetalNative.UnmapBuffer(metalBuf.Handle);
            }

            // Managed 模式需要显式刷新才能让 GPU 看到写入
            if (metalBuf.StorageMode == MetalStorageMode.Managed)
            {
                metalBuf.Flush((ulong)safeOffset, (ulong)copyLength);
            }
        }

        public void Dispose()
        {
            foreach (MetalBuffer metalBuf in _buffers.Values)
            {
                metalBuf.Dispose();
            }

            _buffers.Clear();
        }
    }

    internal sealed class MetalTexture : ITexture
    {
        private static ulong _diagnosticCopyToCount;
        private static ulong _diagnosticCopyRegionCount;
        private static ulong _diagnosticCopyToBufferCount;
        private static ulong _diagnosticDepthUploadSkipCount;

        private nint _handle;
        private readonly nint _deviceHandle;
        private readonly nint _queueHandle;
        private readonly MetalTextureInfo _textureInfo;
        private readonly MetalStorageMode _storageMode;
        private bool _released;

        public TextureCreateInfo Info { get; }

        public BufferRange Storage { get; private set; }

        public int Width => Info.Width;

        public int Height => Info.Height;

        internal nint Handle => _handle;
        /// <summary>
        /// 上传纹理数据到 GPU。
        /// 普通格式通过 CPU-side replaceRegion，深度/模板通过 GPU blit。
        /// </summary>
        private void UploadTextureToGpu(
            nint textureHandle, nint bufferHandle, ulong bufferOffset,
            uint layer, uint level,
            uint regionX, uint regionY, uint regionZ,
            uint regionWidth, uint regionHeight, uint bytesPerRow)
        {
            if (Info.Format.IsDepthOrStencil)
            {
                ulong count = ++_diagnosticDepthUploadSkipCount;
                if (count <= 5 || (count % 100) == 0)
                {
                    Logger.Warning?.PrintMsg(
                        LogClass.Gpu,
                        $"[DIAG] Blit 上传 depth/stencil 纹理: count={count}, format={Info.Format}, size={Info.Width}x{Info.Height}, level={level}");
                }

                MetalResult result = MetalNative.TextureUploadViaBlit(
                    _queueHandle,
                    textureHandle,
                    bufferHandle,
                    bufferOffset,
                    layer, level,
                    regionX, regionY, regionZ,
                    regionWidth, regionHeight,
                    bytesPerRow);

                if (result != MetalResult.Ok)
                {
                    Logger.Error?.PrintMsg(
                        LogClass.Gpu,
                        $"[DIAG] Blit 上传 depth/stencil 纹理失败: result={result}");
                }
            }
            else
            {
                MetalNative.TextureUpload(
                    textureHandle,
                    bufferHandle,
                    bufferOffset,
                    layer, level,
                    regionX, regionY, regionZ,
                    regionWidth, regionHeight,
                    bytesPerRow);
            }
        }

        /// <summary>
        /// 创建 Metal 原生纹理。格式通过 MetalFormatMapping 映射表转换为 MetalPixelFormat。
        /// </summary>
        public MetalTexture(nint deviceHandle, nint queueHandle, TextureCreateInfo info, MetalStorageMode storageMode)
        {
            _deviceHandle = deviceHandle;
            _queueHandle = queueHandle;
            _storageMode = storageMode;
            Info = info;

            MetalTextureType textureType = info.Target.ToMetalTextureType();
            MetalPixelFormat pixelFormat = info.Format.ToMetalFormat();

            if (pixelFormat == MetalPixelFormat.Invalid)
            {
                throw new NotSupportedException(
                    $"Metal 暂不支持纹理格式映射: Format={info.Format}, Target={info.Target}, " +
                    $"Size={info.Width}x{info.Height}x{info.GetDepthOrLayers()}, Levels={info.Levels}, Samples={info.Samples}");
            }

            uint depth = (uint)Math.Max(info.GetDepthOrLayers(), 1);
            uint levels = (uint)Math.Max(info.Levels, 1);
            uint samples = (uint)Math.Max(info.Samples, 1);
            MetalTextureUsage usage = info.DeriveTextureUsage();

            MetalResult result = MetalNative.CreateTexture(
                deviceHandle,
                pixelFormat,
                (uint)info.Width,
                (uint)info.Height,
                depth,
                levels,
                samples,
                textureType,
                usage,
                storageMode,
                out nint handle);

            if (result != MetalResult.Ok)
            {
                throw new InvalidOperationException($"CreateTexture 失败: {result}");
            }

            _handle = handle;

            result = MetalNative.TextureGetInfo(_handle, out MetalTextureInfo texInfo);

            if (result != MetalResult.Ok)
            {
                MetalNative.Release(_handle);
                _handle = nint.Zero;
                throw new InvalidOperationException($"TextureGetInfo 失败: {result}");
            }

            _textureInfo = texInfo;
        }

        // ── CopyTo: 暂为 stub，后续需 BlitEncoder ──

        public void CopyTo(ITexture destination, int firstLayer, int firstLevel)
        {
            ulong count = ++_diagnosticCopyToCount;

            if (count <= 5 || (count % 100) == 0)
            {
                string destinationFormat = destination != null && TryGetMetalFormat(destination, out MetalPixelFormat format)
                    ? format.ToString()
                    : "Unknown";

                Logger.Warning?.PrintMsg(
                    LogClass.Gpu,
                    $"[DIAG] Texture.CopyTo(layer/level) 仍为 stub: count={count}, src={Info.Format}, dst={destinationFormat}, firstLayer={firstLayer}, firstLevel={firstLevel}, size={Info.Width}x{Info.Height}");
            }
        }

        public void CopyTo(ITexture destination, int srcLayer, int dstLayer, int srcLevel, int dstLevel)
        {
            ulong count = ++_diagnosticCopyToCount;

            if (count <= 5 || (count % 100) == 0)
            {
                string destinationFormat = destination != null && TryGetMetalFormat(destination, out MetalPixelFormat format)
                    ? format.ToString()
                    : "Unknown";

                Logger.Warning?.PrintMsg(
                    LogClass.Gpu,
                    $"[DIAG] Texture.CopyTo(single slice) 仍为 stub: count={count}, src={Info.Format}, dst={destinationFormat}, srcLayer={srcLayer}, dstLayer={dstLayer}, srcLevel={srcLevel}, dstLevel={dstLevel}");
            }
        }

        public void CopyTo(ITexture destination, Extents2D srcRegion, Extents2D dstRegion, bool linearFilter)
        {
            ulong count = ++_diagnosticCopyRegionCount;

            if (count <= 5 || (count % 100) == 0)
            {
                string destinationFormat = destination != null && TryGetMetalFormat(destination, out MetalPixelFormat format)
                    ? format.ToString()
                    : "Unknown";

                Logger.Warning?.PrintMsg(
                    LogClass.Gpu,
                    $"[DIAG] Texture.CopyTo(region) 仍为 stub: count={count}, src={Info.Format}, dst={destinationFormat}, src=({srcRegion.X1},{srcRegion.Y1})-({srcRegion.X2},{srcRegion.Y2}), dst=({dstRegion.X1},{dstRegion.Y1})-({dstRegion.X2},{dstRegion.Y2}), linear={linearFilter}");
            }
        }

        public void CopyTo(BufferRange range, int layer, int level, int stride)
        {
            ulong count = ++_diagnosticCopyToBufferCount;

            if (count <= 5 || (count % 100) == 0)
            {
                Logger.Warning?.PrintMsg(
                    LogClass.Gpu,
                    $"[DIAG] Texture.CopyTo(buffer) 仍为 stub: count={count}, format={Info.Format}, layer={layer}, level={level}, stride={stride}, size={range.Size}");
            }
        }

        // ── CreateView: 通过 newTextureView 共享父纹理的底层存储 ──

        public ITexture CreateView(TextureCreateInfo info, int firstLayer, int firstLevel)
        {
            MetalPixelFormat pixelFormat = info.Format.ToMetalFormat();
            MetalTextureType textureType = info.Target.ToMetalTextureType();

            if (pixelFormat == MetalPixelFormat.Invalid)
            {
                throw new NotSupportedException(
                    $"Metal 暂不支持纹理视图格式映射: Format={info.Format}, Target={info.Target}, " +
                    $"FirstLayer={firstLayer}, FirstLevel={firstLevel}");
            }

            uint numLayers = (uint)Math.Max(info.GetDepthOrLayers(), 1);
            uint numLevels = (uint)Math.Max(info.Levels, 1);

            MetalResult result = MetalNative.CreateTextureView(
                _handle,
                pixelFormat,
                textureType,
                (uint)firstLayer,
                numLayers,
                (uint)firstLevel,
                numLevels,
                out nint viewHandle);

            if (result != MetalResult.Ok)
            {
                Logger.Warning?.PrintMsg(
                    LogClass.Gpu,
                    $"[DIAG] CreateTextureView 失败，回退为独立纹理: result={result}, format={info.Format}, target={info.Target}, size={info.Width}x{info.Height}, firstLayer={firstLayer}, firstLevel={firstLevel}, layers={numLayers}, levels={numLevels}");

                // 回退：创建全新的纹理（不支持 view 的格式/类型组合时）
                return new MetalTexture(_deviceHandle, _queueHandle, info, _storageMode);
            }

            // 创建轻量封装：传递 firstLayer/firstLevel 供区域上传计算父纹理偏移
            return new MetalTextureViewProxy(
                viewHandle, info, _deviceHandle, this,
                firstLayer, firstLevel);
        }

        /// <summary>
        /// 纹理视图的轻量封装。共享父纹理的 Metal 纹理存储，Release 时只释放 view 句柄。
        /// 区域上传通过父纹理 handle + 父纹理相对坐标执行，确保写入正确的底层存储位置。
        /// </summary>
        private sealed class MetalTextureViewProxy : ITexture
        {
            private nint _handle;
            private readonly nint _deviceHandle;
            private readonly int _firstLayer;
            private readonly int _firstLevel;
            private readonly MetalTexture _parent;
            private bool _released;

            public TextureCreateInfo Info { get; }
            public BufferRange Storage { get; set; }
            public int Width => Info.Width;
            public int Height => Info.Height;
            internal nint Handle => _handle;

            public MetalTextureViewProxy(
                nint handle,
                TextureCreateInfo info,
                nint deviceHandle,
                MetalTexture parent,
                int firstLayer,
                int firstLevel)
            {
                _handle = handle;
                _deviceHandle = deviceHandle;
                _firstLayer = firstLayer;
                _firstLevel = firstLevel;
                _parent = parent;
                Info = info;
            }

            public void CopyTo(ITexture destination, int firstLayer, int firstLevel) => _parent.CopyTo(destination, firstLayer, firstLevel);
            public void CopyTo(ITexture destination, int srcLayer, int dstLayer, int srcLevel, int dstLevel) => _parent.CopyTo(destination, srcLayer, dstLayer, srcLevel, dstLevel);
            public void CopyTo(ITexture destination, Extents2D srcRegion, Extents2D dstRegion, bool linearFilter) => _parent.CopyTo(destination, srcRegion, dstRegion, linearFilter);
            public void CopyTo(BufferRange range, int layer, int level, int stride) => _parent.CopyTo(range, layer, level, stride);

            public ITexture CreateView(TextureCreateInfo info, int firstLayer, int firstLevel)
            {
                // 视图的视图：直接委托给父纹理的 CreateView
                return _parent.CreateView(info, firstLayer, firstLevel);
            }

            public PinnedSpan<byte> GetData() => GetData(0, 0);

            public PinnedSpan<byte> GetData(int layer, int level)
            {
                if (_handle == nint.Zero || _released)
                    return default;

                int safeLevel = Math.Clamp(level, 0, Info.Levels - 1);
                int safeLayer = Math.Clamp(layer, 0, Math.Max(Info.GetDepthOrLayers() - 1, 0));

                // 转换为父纹理中的绝对坐标
                int parentLevel = _firstLevel + safeLevel;
                int parentLayer = _firstLayer + safeLayer;

                int stride = Info.GetMipStride(safeLevel);
                int dataSize = Info.GetMipSize2D(safeLevel);

                if (dataSize <= 0)
                    return default;

                MetalResult result = MetalNative.CreateBuffer(
                    _deviceHandle, (ulong)dataSize, MetalStorageMode.Shared, out nint tempBuf);

                if (result != MetalResult.Ok)
                    return default;

                try
                {
                    // 使用父纹理 handle + 父纹理相对坐标
                    result = MetalNative.TextureReadback(
                        _parent._handle, tempBuf, 0, (uint)parentLayer, (uint)parentLevel, (uint)stride);

                    if (result != MetalResult.Ok)
                        return default;

                    result = MetalNative.MapBuffer(tempBuf, out nint ptr);
                    if (result != MetalResult.Ok)
                        return default;

                    try
                    {
                        byte[] data = new byte[dataSize];
                        Marshal.Copy(ptr, data, 0, dataSize);
                        return PinnedSpan<byte>.UnsafeFromSpan(data.AsSpan(0, dataSize));
                    }
                    finally
                    {
                        MetalNative.UnmapBuffer(tempBuf);
                    }
                }
                finally
                {
                    MetalNative.Release(tempBuf);
                }
            }

            public void Release()
            {
                if (!_released && _handle != nint.Zero)
                {
                    MetalNative.Release(_handle);
                    _handle = nint.Zero;
                    _released = true;
                }
            }

            public void SetData(MemoryOwner<byte> data) => SetData(data, 0, 0);

            public void SetData(MemoryOwner<byte> data, int layer, int level)
            {
                if (_handle == nint.Zero || _released)
                {
                    data.Dispose();
                    return;
                }

                try
                {




                    int safeLevel = Math.Clamp(level, 0, Info.Levels - 1);
                    int safeLayer = Math.Clamp(layer, 0, Math.Max(Info.GetDepthOrLayers() - 1, 0));

                    // 转换为父纹理中的绝对坐标
                    int parentLevel = _firstLevel + safeLevel;
                    int parentLayer = _firstLayer + safeLayer;

                    int levelWidth = Math.Max(1, _parent.Info.Width >> parentLevel);
                    int levelHeight = Math.Max(1, _parent.Info.Height >> parentLevel);
                    int stride = Info.GetMipStride(safeLevel);
                    int dataSize = Info.GetMipSize2D(safeLevel);

                    if (dataSize <= 0) return;

                    unsafe
                    {
                        fixed (byte* pData = data.Span)
                        {
                            MetalResult mr = MetalNative.CreateBufferWithBytes(
                                _deviceHandle, (nint)pData, (ulong)dataSize,
                                MetalStorageMode.Shared, out nint tempBuf);

                            if (mr != MetalResult.Ok) return;

                            try
                            {
                                // 使用父纹理 handle + 父纹理相对坐标
                                _parent.UploadTextureToGpu(
                                    _parent._handle, tempBuf, 0,
                                    (uint)parentLayer, (uint)parentLevel,
                                    0, 0, 0,
                                    (uint)levelWidth, (uint)levelHeight,
                                    (uint)stride);
                            }
                            finally
                            {
                                MetalNative.Release(tempBuf);
                            }
                        }
                    }
                }
                finally
                {
                    data.Dispose();
                }
            }

            public void SetData(MemoryOwner<byte> data, int layer, int level, Rectangle<int> region)
            {
                if (_handle == nint.Zero || _released)
                {
                    data.Dispose();
                    return;
                }

                try
                {






                    int safeLevel = Math.Clamp(level, 0, Info.Levels - 1);
                    int safeLayer = Math.Clamp(layer, 0, Math.Max(Info.GetDepthOrLayers() - 1, 0));

                    // 将 view 中的 layer/level 转换为父纹理中的绝对坐标
                    int parentLevel = _firstLevel + safeLevel;
                    int parentLayer = _firstLayer + safeLayer;

                    // 使用父纹理的尺寸计算 mip 级尺寸，确保 region 不越界
                    int parentLevelWidth = Math.Max(1, _parent.Info.Width >> parentLevel);
                    int parentLevelHeight = Math.Max(1, _parent.Info.Height >> parentLevel);

                    int regionX = Math.Clamp(region.X, 0, parentLevelWidth - 1);
                    int regionY = Math.Clamp(region.Y, 0, parentLevelHeight - 1);
                    int regionW = Math.Clamp(region.Width, 0, parentLevelWidth - regionX);
                    int regionH = Math.Clamp(region.Height, 0, parentLevelHeight - regionY);

                    if (regionW <= 0 || regionH <= 0)
                        return;

                    int bytesPerPixel = _parent.Info.BytesPerPixel;
                    int regionDataSize = regionW * regionH * bytesPerPixel;

                    if (regionDataSize <= 0)
                        return;

                    unsafe
                    {
                        fixed (byte* pData = data.Span)
                        {
                            MetalResult mr = MetalNative.CreateBufferWithBytes(
                                _deviceHandle, (nint)pData, (ulong)regionDataSize,
                                MetalStorageMode.Shared, out nint tempBuf);

                            if (mr != MetalResult.Ok) return;

                            try
                            {
                                // 使用父纹理 handle + 父纹理相对坐标
                                _parent.UploadTextureToGpu(
                                    _parent._handle,
                                    tempBuf,
                                    0,
                                    (uint)parentLayer,
                                    (uint)parentLevel,
                                    (uint)regionX,
                                    (uint)regionY,
                                    0,
                                    (uint)regionW,
                                    (uint)regionH,
                                    (uint)(regionW * bytesPerPixel));
                            }
                            finally
                            {
                                MetalNative.Release(tempBuf);
                            }
                        }
                    }
                }
                finally
                {
                    data.Dispose();
                }
            }

            public void SetStorage(BufferRange buffer)
            {
                Storage = buffer;
            }
        }

        // ── GetData: TextureReadback → MapBuffer → Marshal.Copy → UnmapBuffer ──

        public PinnedSpan<byte> GetData()
        {
            return GetData(0, 0);
        }

        public PinnedSpan<byte> GetData(int layer, int level)
        {
            if (_handle == nint.Zero || _released)
            {
                return default;
            }

            int safeLevel = Math.Clamp(level, 0, Info.Levels - 1);
            int safeLayer = Math.Clamp(layer, 0, Math.Max((int)_textureInfo.Depth - 1, 0));
            int stride = Info.GetMipStride(safeLevel);
            int levelHeight = Math.Max(1, Info.Height >> safeLevel);
            int dataSize = Info.GetMipSize2D(safeLevel);

            if (dataSize <= 0)
            {
                return default;
            }

            // 创建临时 MetalBuffer 接收回读数据
            MetalResult result = MetalNative.CreateBuffer(
                _deviceHandle, (ulong)dataSize, MetalStorageMode.Shared, out nint tempBuf);

            if (result != MetalResult.Ok)
            {
                return default;
            }

            try
            {
                result = MetalNative.TextureReadback(
                    _handle, tempBuf, 0, (uint)safeLayer, (uint)safeLevel, (uint)stride);

                if (result != MetalResult.Ok)
                {
                    return default;
                }

                result = MetalNative.MapBuffer(tempBuf, out nint ptr);

                if (result != MetalResult.Ok)
                {
                    return default;
                }

                try
                {
                    byte[] data = new byte[dataSize];
                    Marshal.Copy(ptr, data, 0, dataSize);
                    return PinnedSpan<byte>.UnsafeFromSpan(data.AsSpan(0, dataSize));
                }
                finally
                {
                    MetalNative.UnmapBuffer(tempBuf);
                }
            }
            finally
            {
                MetalNative.Release(tempBuf);
            }
        }

        // ── Release ──

        public void Release()
        {
            if (!_released && _handle != nint.Zero)
            {
                MetalNative.Release(_handle);
                _handle = nint.Zero;
            }

            _released = true;
        }

        internal static bool TryGetNativeHandle(ITexture texture, out nint handle)
        {
            switch (texture)
            {
                case MetalTexture metalTexture:
                    handle = metalTexture._handle;
                    return handle != nint.Zero;
                case MetalTextureViewProxy textureView:
                    handle = textureView.Handle;
                    return handle != nint.Zero;
                default:
                    handle = nint.Zero;
                    return false;
            }
        }

        internal static bool TryGetMetalFormat(ITexture texture, out MetalPixelFormat format)
        {
            switch (texture)
            {
                case MetalTexture metalTexture:
                    format = metalTexture.Info.Format.ToMetalFormat();
                    return format != MetalPixelFormat.Invalid;
                case MetalTextureViewProxy textureView:
                    format = textureView.Info.Format.ToMetalFormat();
                    return format != MetalPixelFormat.Invalid;
                default:
                    format = MetalPixelFormat.Invalid;
                    return false;
            }
        }

        // ── SetData: pin → CreateBufferWithBytes → TextureUpload → release temp buf ──

        public void SetData(MemoryOwner<byte> data)
        {
            SetData(data, 0, 0);
        }

        public void SetData(MemoryOwner<byte> data, int layer, int level)
        {
            if (_handle == nint.Zero || _released)
            {
                data.Dispose();
                return;
            }

            try
            {






                int safeLevel = Math.Clamp(level, 0, Info.Levels - 1);
                int safeLayer = Math.Clamp(layer, 0, Math.Max((int)_textureInfo.Depth - 1, 0));
                int levelWidth = Math.Max(1, Info.Width >> safeLevel);
                int levelHeight = Math.Max(1, Info.Height >> safeLevel);
                int stride = Info.GetMipStride(safeLevel);
                int dataSize = Info.GetMipSize2D(safeLevel);

                if (dataSize <= 0)
                {
                    return;
                }

                unsafe
                {
                    fixed (byte* pData = data.Span)
                    {
                        MetalResult result = MetalNative.CreateBufferWithBytes(
                            _deviceHandle,
                            (nint)pData,
                            (ulong)dataSize,
                            MetalStorageMode.Shared,
                            out nint tempBuf);

                        if (result != MetalResult.Ok)
                        {
                            return;
                        }

                        try
                        {
                            this.UploadTextureToGpu(
                                _handle,
                                tempBuf,
                                0,
                                (uint)safeLayer,
                                (uint)safeLevel,
                                0, 0, 0,
                                (uint)levelWidth,
                                (uint)levelHeight,
                                (uint)stride);
                        }
                        finally
                        {
                            MetalNative.Release(tempBuf);
                        }
                    }
                }
            }
            finally
            {
                data.Dispose();
            }
        }

        public void SetData(MemoryOwner<byte> data, int layer, int level, Rectangle<int> region)
        {
            if (_handle == nint.Zero || _released)
            {
                data.Dispose();
                return;
            }

            try
            {






                int safeLevel = Math.Clamp(level, 0, Info.Levels - 1);
                int safeLayer = Math.Clamp(layer, 0, Math.Max((int)_textureInfo.Depth - 1, 0));
                int levelHeight = Math.Max(1, Info.Height >> safeLevel);
                int levelWidth = Math.Max(1, Info.Width >> safeLevel);

                int regionX = Math.Clamp(region.X, 0, levelWidth - 1);
                int regionY = Math.Clamp(region.Y, 0, levelHeight - 1);
                int regionW = Math.Clamp(region.Width, 0, levelWidth - regionX);
                int regionH = Math.Clamp(region.Height, 0, levelHeight - regionY);

                if (regionW <= 0 || regionH <= 0)
                {
                    return;
                }

                int regionDataSize = regionW * regionH * Info.BytesPerPixel;

                if (regionDataSize <= 0)
                {
                    return;
                }

                unsafe
                {
                    fixed (byte* pData = data.Span)
                    {
                        MetalResult result = MetalNative.CreateBufferWithBytes(
                            _deviceHandle,
                            (nint)pData,
                            (ulong)regionDataSize,
                            MetalStorageMode.Shared,
                            out nint tempBuf);

                        if (result != MetalResult.Ok)
                        {
                            return;
                        }

                        try
                        {
                            this.UploadTextureToGpu(
                                _handle,
                                tempBuf,
                                0,
                                (uint)safeLayer,
                                (uint)safeLevel,
                                (uint)regionX,
                                (uint)regionY,
                                0,
                                (uint)regionW,
                                (uint)regionH,
                                (uint)(regionW * Info.BytesPerPixel));
                        }
                        finally
                        {
                            MetalNative.Release(tempBuf);
                        }
                    }
                }
            }
            finally
            {
                data.Dispose();
            }
        }

        public void SetStorage(BufferRange buffer)
        {
            Storage = buffer;
        }
    }

    internal sealed class MetalSampler : ISampler
    {
        private nint _handle;
        private bool _disposed;

        public SamplerCreateInfo Info { get; }

        public nint Handle => _handle;

        public MetalSampler(nint deviceHandle, SamplerCreateInfo info)
        {
            Info = info;

            MetalSamplerDescriptor desc = MapSamplerCreateInfo(info);

            MetalResult result = MetalNative.CreateSampler(deviceHandle, desc, out nint handle);

            if (result != MetalResult.Ok)
            {
                throw new InvalidOperationException($"CreateSampler 失败: {result}");
            }

            _handle = handle;
        }

        /// <summary>
        /// 将 GAL SamplerCreateInfo 映射到 MetalSamplerDescriptor。
        /// </summary>
        private static MetalSamplerDescriptor MapSamplerCreateInfo(SamplerCreateInfo info)
        {
            MetalSamplerDescriptor desc = new();

            // MinFilter → min_filter + mip_filter
            (desc.MinFilter, desc.MipFilter) = info.MinFilter switch
            {
                MinFilter.Nearest => (MetalSamplerMinMagFilter.Nearest, MetalSamplerMipFilter.NotMipmapped),
                MinFilter.Linear => (MetalSamplerMinMagFilter.Linear, MetalSamplerMipFilter.NotMipmapped),
                MinFilter.NearestMipmapNearest => (MetalSamplerMinMagFilter.Nearest, MetalSamplerMipFilter.Nearest),
                MinFilter.LinearMipmapNearest => (MetalSamplerMinMagFilter.Linear, MetalSamplerMipFilter.Nearest),
                MinFilter.NearestMipmapLinear => (MetalSamplerMinMagFilter.Nearest, MetalSamplerMipFilter.Linear),
                MinFilter.LinearMipmapLinear => (MetalSamplerMinMagFilter.Linear, MetalSamplerMipFilter.Linear),
                _ => (MetalSamplerMinMagFilter.Nearest, MetalSamplerMipFilter.NotMipmapped),
            };

            // MagFilter → mag_filter
            desc.MagFilter = info.MagFilter switch
            {
                MagFilter.Nearest => MetalSamplerMinMagFilter.Nearest,
                MagFilter.Linear => MetalSamplerMinMagFilter.Linear,
                _ => MetalSamplerMinMagFilter.Nearest,
            };

            // AddressMode → address_s/t/r
            desc.AddressS = MapAddressMode(info.AddressU);
            desc.AddressT = MapAddressMode(info.AddressV);
            desc.AddressR = MapAddressMode(info.AddressP);

            // CompareMode + CompareOp → compare_function
            desc.CompareFunction = info.CompareMode switch
            {
                CompareMode.None => MetalCompareFunction.Always,  // Always = 禁用比较
                CompareMode.CompareRToTexture => MapCompareOp(info.CompareOp),
                _ => MetalCompareFunction.Always,
            };

            desc.MaxAnisotropy = info.MaxAnisotropy;
            desc.LodMinClamp = info.MinLod;
            desc.LodMaxClamp = info.MaxLod;
            desc.NormalizedCoordinates = true; // GAL 默认使用归一化坐标

            return desc;
        }

        private static MetalSamplerAddressMode MapAddressMode(AddressMode mode)
        {
            return mode switch
            {
                AddressMode.Repeat => MetalSamplerAddressMode.Repeat,
                AddressMode.MirroredRepeat => MetalSamplerAddressMode.MirroredRepeat,
                AddressMode.ClampToEdge => MetalSamplerAddressMode.ClampToEdge,
                AddressMode.ClampToBorder => MetalSamplerAddressMode.ClampToBorderColor,
                AddressMode.Clamp => MetalSamplerAddressMode.ClampToZero,
                AddressMode.MirrorClampToEdge => MetalSamplerAddressMode.MirrorClampToEdge,
                AddressMode.MirrorClampToBorder => MetalSamplerAddressMode.ClampToBorderColor,
                AddressMode.MirrorClamp => MetalSamplerAddressMode.MirroredRepeat,
                _ => MetalSamplerAddressMode.ClampToEdge,
            };
        }

        private static MetalCompareFunction MapCompareOp(CompareOp op)
        {
            return op switch
            {
                CompareOp.Never => MetalCompareFunction.Never,
                CompareOp.Less => MetalCompareFunction.Less,
                CompareOp.Equal => MetalCompareFunction.Equal,
                CompareOp.LessOrEqual => MetalCompareFunction.LessEqual,
                CompareOp.Greater => MetalCompareFunction.Greater,
                CompareOp.NotEqual => MetalCompareFunction.NotEqual,
                CompareOp.GreaterOrEqual => MetalCompareFunction.GreaterEqual,
                CompareOp.Always => MetalCompareFunction.Always,
                _ => MetalCompareFunction.Always,
            };
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

    internal sealed class MetalTextureArray : ITextureArray
    {
        private readonly ISampler[] _samplers;
        private readonly ITexture[] _textures;

        public bool IsBuffer { get; }

        public MetalTextureArray(int size, bool isBuffer)
        {
            _samplers = new ISampler[Math.Max(size, 0)];
            _textures = new ITexture[Math.Max(size, 0)];
            IsBuffer = isBuffer;
        }

        public void Dispose()
        {
        }

        public void SetSamplers(int index, ISampler[] samplers)
        {
            Copy(index, samplers, _samplers);
        }

        public void SetTextures(int index, ITexture[] textures)
        {
            Copy(index, textures, _textures);
        }

        private static void Copy<T>(int index, T[] source, T[] destination)
        {
            if (index < 0 || index >= destination.Length)
            {
                return;
            }

            int copyLength = Math.Min(source.Length, destination.Length - index);

            if (copyLength > 0)
            {
                Array.Copy(source, 0, destination, index, copyLength);
            }
        }
    }

    internal sealed class MetalImageArray : IImageArray
    {
        private readonly ITexture[] _images;

        public bool IsBuffer { get; }

        public MetalImageArray(int size, bool isBuffer)
        {
            _images = new ITexture[Math.Max(size, 0)];
            IsBuffer = isBuffer;
        }

        public void Dispose()
        {
        }

        public void SetImages(int index, ITexture[] images)
        {
            if (index < 0 || index >= _images.Length)
            {
                return;
            }

            int copyLength = Math.Min(images.Length, _images.Length - index);

            if (copyLength > 0)
            {
                Array.Copy(images, 0, _images, index, copyLength);
            }
        }
    }
}
