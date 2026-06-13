using Ryujinx.Common.Memory;
using Ryujinx.Graphics.GAL;
using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;

namespace Ryujinx.Graphics.Metal
{
    internal sealed class MetalBufferPool
    {
        private readonly Dictionary<int, byte[]> _buffers = [];
        private int _nextHandle = 1;

        public BufferHandle Create(int size, BufferAccess access)
        {
            return CreateCore(Math.Max(size, 0));
        }

        public BufferHandle CreateImported(nint pointer, int size)
        {
            return CreateCore(Math.Max(size, 0));
        }

        public BufferHandle CreateSparse(ReadOnlySpan<BufferRange> storageBuffers)
        {
            int totalSize = 0;

            foreach (BufferRange range in storageBuffers)
            {
                totalSize += Math.Max(range.Size, 0);
            }

            return CreateCore(totalSize);
        }

        public void Delete(BufferHandle buffer)
        {
            _buffers.Remove((int)buffer);
        }

        public PinnedSpan<byte> GetData(BufferHandle buffer, int offset, int size)
        {
            if (!_buffers.TryGetValue((int)buffer, out byte[] data))
            {
                return default;
            }

            int safeOffset = Math.Clamp(offset, 0, data.Length);
            int safeLength = Math.Clamp(size, 0, data.Length - safeOffset);

            return PinnedSpan<byte>.UnsafeFromSpan(data.AsSpan(safeOffset, safeLength));
        }

        public void SetData(BufferHandle buffer, int offset, ReadOnlySpan<byte> data)
        {
            if (!_buffers.TryGetValue((int)buffer, out byte[] destination))
            {
                return;
            }

            int safeOffset = Math.Clamp(offset, 0, destination.Length);
            int copyLength = Math.Min(data.Length, destination.Length - safeOffset);

            if (copyLength > 0)
            {
                data[..copyLength].CopyTo(destination.AsSpan(safeOffset, copyLength));
            }
        }

        private BufferHandle CreateCore(int size)
        {
            int handleValue = _nextHandle++;
            _buffers.Add(handleValue, new byte[size]);

            ulong rawHandle = (uint)handleValue;
            return Unsafe.As<ulong, BufferHandle>(ref rawHandle);
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
