using Ryujinx.Common.Memory;
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
            // 暂不支持稀疏缓冲区，回退为分配总大小的普通缓冲区
            long totalSize = 0;

            foreach (BufferRange range in storageBuffers)
            {
                totalSize += Math.Max(range.Size, 0);
            }

            return Create((int)totalSize, BufferAccess.Default);
        }

        public void Delete(BufferHandle buffer)
        {
            if (_buffers.Remove((int)buffer, out MetalBuffer metalBuf))
            {
                metalBuf.Dispose();
            }
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
        private readonly List<byte[]> _levelData;

        public TextureCreateInfo Info { get; }

        public BufferRange Storage { get; private set; }

        public int Width => Info.Width;

        public int Height => Info.Height;

        public MetalTexture(TextureCreateInfo info)
        {
            Info = info;
            _levelData = new List<byte[]>(Math.Max(1, info.Levels));

            for (int level = 0; level < Math.Max(1, info.Levels); level++)
            {
                _levelData.Add(new byte[Math.Max(info.GetMipSize(level), 0)]);
            }
        }

        public void CopyTo(ITexture destination, int firstLayer, int firstLevel)
        {
        }

        public void CopyTo(ITexture destination, int srcLayer, int dstLayer, int srcLevel, int dstLevel)
        {
        }

        public void CopyTo(ITexture destination, Extents2D srcRegion, Extents2D dstRegion, bool linearFilter)
        {
        }

        public void CopyTo(BufferRange range, int layer, int level, int stride)
        {
        }

        public ITexture CreateView(TextureCreateInfo info, int firstLayer, int firstLevel)
        {
            return new MetalTexture(info);
        }

        public PinnedSpan<byte> GetData()
        {
            return GetData(0, 0);
        }

        public PinnedSpan<byte> GetData(int layer, int level)
        {
            int safeLevel = Math.Clamp(level, 0, _levelData.Count - 1);
            return PinnedSpan<byte>.UnsafeFromSpan(_levelData[safeLevel]);
        }

        public void Release()
        {
        }

        public void SetData(MemoryOwner<byte> data)
        {
            SetData(data, 0, 0);
        }

        public void SetData(MemoryOwner<byte> data, int layer, int level)
        {
            try
            {
                int safeLevel = Math.Clamp(level, 0, _levelData.Count - 1);
                byte[] destination = _levelData[safeLevel];
                ReadOnlySpan<byte> source = data.Span;
                int copyLength = Math.Min(source.Length, destination.Length);

                if (copyLength > 0)
                {
                    source[..copyLength].CopyTo(destination);
                }
            }
            finally
            {
                data.Dispose();
            }
        }

        public void SetData(MemoryOwner<byte> data, int layer, int level, Rectangle<int> region)
        {
            SetData(data, layer, level);
        }

        public void SetStorage(BufferRange buffer)
        {
            Storage = buffer;
        }
    }

    internal sealed class MetalSampler : ISampler
    {
        public SamplerCreateInfo Info { get; }

        public MetalSampler(SamplerCreateInfo info)
        {
            Info = info;
        }

        public void Dispose()
        {
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
