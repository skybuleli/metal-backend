using Ryujinx.Graphics.GAL;
using System;

namespace Ryujinx.Graphics.Metal
{
    /// <summary>
    /// GAL 顶点输入状态到 Metal 顶点描述符的集中映射。
    ///
    /// 这层负责把顶点属性、顶点缓冲布局和格式换算统一拼成
    /// <see cref="MetalRenderPipelineDescriptor"/> 可直接消费的数据，
    /// 让 <see cref="MetalPipeline"/> 只保留缓存与重建触发逻辑。
    /// </summary>
    internal static class MetalVertexDescriptorMapping
    {
        public static void PopulateVertexLayout(
            ref MetalRenderPipelineDescriptor descriptor,
            ReadOnlySpan<VertexAttribDescriptor> vertexAttribs,
            ReadOnlySpan<VertexBufferDescriptor> vertexBuffers,
            int maxVertexAttributes,
            int maxVertexBufferBindings,
            int zeroVertexBufferIndex,
            int firstUserVertexBufferIndex,
            uint defaultVertexStride)
        {
            int attrCount = vertexAttribs.Length;
            if (attrCount > maxVertexAttributes)
            {
                attrCount = maxVertexAttributes;
            }

            int requiredBufferCount = vertexBuffers.Length;
            int layoutCount = 0;
            bool usesZeroVertexBuffer = false;
            Span<uint> referencedStrides = stackalloc uint[maxVertexBufferBindings];

            for (int i = 0; i < attrCount; i++)
            {
                VertexAttribDescriptor attrib = vertexAttribs[i];
                if (!TryConvertVertexFormat(attrib.Format, out MetalVertexFormat vertexFormat))
                {
                    continue;
                }

                int sourceBufferIndex = attrib.BufferIndex;
                if (!attrib.IsZero && (uint)sourceBufferIndex >= maxVertexBufferBindings)
                {
                    continue;
                }

                int metalBufferIndex;
                if (attrib.IsZero)
                {
                    usesZeroVertexBuffer = true;
                    metalBufferIndex = zeroVertexBufferIndex;
                    layoutCount = Math.Max(layoutCount, zeroVertexBufferIndex + 1);
                }
                else
                {
                    requiredBufferCount = Math.Max(requiredBufferCount, sourceBufferIndex + 1);
                    metalBufferIndex = sourceBufferIndex + firstUserVertexBufferIndex;
                    layoutCount = Math.Max(layoutCount, metalBufferIndex + 1);

                    uint requiredStride = (uint)Math.Max(attrib.Offset, 0) + GetVertexFormatSize(attrib.Format);
                    referencedStrides[sourceBufferIndex] = Math.Max(referencedStrides[sourceBufferIndex], requiredStride);
                }

                descriptor.VertexAttributes[i] = new MetalVertexAttributeDescriptor
                {
                    AttributeIndex = (uint)i,
                    BufferIndex = (uint)metalBufferIndex,
                    Format = vertexFormat,
                    Offset = attrib.IsZero ? 0u : (uint)Math.Max(attrib.Offset, 0),
                };
            }

            descriptor.VertexAttributeCount = (uint)attrCount;

            int bufferCount = requiredBufferCount;
            if (bufferCount > maxVertexBufferBindings)
            {
                bufferCount = maxVertexBufferBindings;
            }

            if (usesZeroVertexBuffer)
            {
                descriptor.VertexBufferLayouts[zeroVertexBufferIndex] = new MetalVertexBufferLayoutDescriptor
                {
                    BufferIndex = (uint)zeroVertexBufferIndex,
                    Stride = defaultVertexStride,
                    StepFunction = MetalVertexStepFunction.Constant,
                    StepRate = 0,
                };
            }

            for (int i = 0; i < bufferCount; i++)
            {
                VertexBufferDescriptor buffer = i < vertexBuffers.Length ? vertexBuffers[i] : default;
                uint stride = (uint)Math.Max(buffer.Stride, 0);
                if (stride == 0 && referencedStrides[i] != 0)
                {
                    stride = Math.Max(referencedStrides[i], defaultVertexStride);
                }

                descriptor.VertexBufferLayouts[i + firstUserVertexBufferIndex] = new MetalVertexBufferLayoutDescriptor
                {
                    BufferIndex = (uint)(i + firstUserVertexBufferIndex),
                    Stride = stride,
                    StepFunction = buffer.Divisor != 0
                        ? MetalVertexStepFunction.PerInstance
                        : MetalVertexStepFunction.PerVertex,
                    StepRate = buffer.Divisor != 0 ? (uint)Math.Max(buffer.Divisor, 1) : 1u,
                };
            }

            layoutCount = Math.Max(layoutCount, bufferCount + firstUserVertexBufferIndex);
            descriptor.VertexBufferLayoutCount = (uint)Math.Min(layoutCount, maxVertexBufferBindings);
        }

        public static bool TryConvertVertexFormat(Format format, out MetalVertexFormat metalFormat)
        {
            metalFormat = format switch
            {
                Format.R8Unorm => MetalVertexFormat.UCharNormalized,
                Format.R8Snorm => MetalVertexFormat.CharNormalized,
                Format.R8Uint or Format.R8Uscaled => MetalVertexFormat.UChar,
                Format.R8Sint or Format.R8Sscaled => MetalVertexFormat.Char,
                Format.R16Float => MetalVertexFormat.Half,
                Format.R16Unorm => MetalVertexFormat.UShortNormalized,
                Format.R16Snorm => MetalVertexFormat.ShortNormalized,
                Format.R16Uint or Format.R16Uscaled => MetalVertexFormat.UShort,
                Format.R16Sint or Format.R16Sscaled => MetalVertexFormat.Short,
                Format.R32Float => MetalVertexFormat.Float,
                Format.R32Uint or Format.R32Uscaled => MetalVertexFormat.UInt,
                Format.R32Sint or Format.R32Sscaled => MetalVertexFormat.Int,
                Format.R8G8Unorm => MetalVertexFormat.UChar2Normalized,
                Format.R8G8Snorm => MetalVertexFormat.Char2Normalized,
                Format.R8G8Uint or Format.R8G8Uscaled => MetalVertexFormat.UChar2,
                Format.R8G8Sint or Format.R8G8Sscaled => MetalVertexFormat.Char2,
                Format.R16G16Float => MetalVertexFormat.Half2,
                Format.R16G16Unorm => MetalVertexFormat.UShort2Normalized,
                Format.R16G16Snorm => MetalVertexFormat.Short2Normalized,
                Format.R16G16Uint or Format.R16G16Uscaled => MetalVertexFormat.UShort2,
                Format.R16G16Sint or Format.R16G16Sscaled => MetalVertexFormat.Short2,
                Format.R32G32Float => MetalVertexFormat.Float2,
                Format.R32G32Uint or Format.R32G32Uscaled => MetalVertexFormat.UInt2,
                Format.R32G32Sint or Format.R32G32Sscaled => MetalVertexFormat.Int2,
                Format.R8G8B8A8Unorm => MetalVertexFormat.UChar4Normalized,
                Format.R8G8B8A8Snorm => MetalVertexFormat.Char4Normalized,
                Format.R8G8B8A8Uint or Format.R8G8B8A8Uscaled => MetalVertexFormat.UChar4,
                Format.R8G8B8A8Sint or Format.R8G8B8A8Sscaled => MetalVertexFormat.Char4,
                Format.R16G16B16A16Float => MetalVertexFormat.Half4,
                Format.R16G16B16A16Unorm => MetalVertexFormat.UShort4Normalized,
                Format.R16G16B16A16Snorm => MetalVertexFormat.Short4Normalized,
                Format.R16G16B16A16Uint or Format.R16G16B16A16Uscaled => MetalVertexFormat.UShort4,
                Format.R16G16B16A16Sint or Format.R16G16B16A16Sscaled => MetalVertexFormat.Short4,
                Format.R32G32B32A32Float => MetalVertexFormat.Float4,
                Format.R32G32B32A32Uint or Format.R32G32B32A32Uscaled => MetalVertexFormat.UInt4,
                Format.R32G32B32A32Sint or Format.R32G32B32A32Sscaled => MetalVertexFormat.Int4,
                Format.R10G10B10A2Unorm => MetalVertexFormat.UInt1010102Normalized,
                Format.R10G10B10A2Snorm => MetalVertexFormat.Int1010102Normalized,
                Format.B8G8R8A8Unorm => MetalVertexFormat.UChar4NormalizedBgra,
                _ => MetalVertexFormat.Invalid,
            };

            return metalFormat != MetalVertexFormat.Invalid;
        }

        public static uint GetVertexFormatSize(Format format)
        {
            return format switch
            {
                Format.R8Unorm or Format.R8Snorm or Format.R8Uint or Format.R8Sint
                    or Format.R8Uscaled or Format.R8Sscaled => 1,
                Format.R16Float or Format.R16Unorm or Format.R16Snorm or Format.R16Uint or Format.R16Sint
                    or Format.R16Uscaled or Format.R16Sscaled => 2,
                Format.R32Float or Format.R32Uint or Format.R32Sint
                    or Format.R32Uscaled or Format.R32Sscaled
                    or Format.R10G10B10A2Unorm or Format.R10G10B10A2Snorm
                    or Format.R10G10B10A2Uint or Format.R10G10B10A2Sint
                    or Format.R10G10B10A2Uscaled or Format.R10G10B10A2Sscaled
                    or Format.R11G11B10Float or Format.R9G9B9E5Float => 4,
                Format.R8G8Unorm or Format.R8G8Snorm or Format.R8G8Uint or Format.R8G8Sint
                    or Format.R8G8Uscaled or Format.R8G8Sscaled => 2,
                Format.R16G16Float or Format.R16G16Unorm or Format.R16G16Snorm or Format.R16G16Uint
                    or Format.R16G16Sint or Format.R16G16Uscaled or Format.R16G16Sscaled => 4,
                Format.R32G32Float or Format.R32G32Uint or Format.R32G32Sint
                    or Format.R32G32Uscaled or Format.R32G32Sscaled => 8,
                Format.R8G8B8Unorm or Format.R8G8B8Snorm or Format.R8G8B8Uint or Format.R8G8B8Sint
                    or Format.R8G8B8Uscaled or Format.R8G8B8Sscaled => 3,
                Format.R16G16B16Float or Format.R16G16B16Unorm or Format.R16G16B16Snorm or Format.R16G16B16Uint
                    or Format.R16G16B16Sint or Format.R16G16B16Uscaled or Format.R16G16B16Sscaled => 6,
                Format.R32G32B32Float or Format.R32G32B32Uint or Format.R32G32B32Sint
                    or Format.R32G32B32Uscaled or Format.R32G32B32Sscaled => 12,
                Format.R8G8B8A8Unorm or Format.R8G8B8A8Snorm or Format.R8G8B8A8Uint or Format.R8G8B8A8Sint
                    or Format.R8G8B8A8Uscaled or Format.R8G8B8A8Sscaled
                    or Format.B8G8R8A8Unorm or Format.B8G8R8A8Srgb
                    or Format.R4G4B4A4Unorm or Format.R5G5B5X1Unorm or Format.R5G5B5A1Unorm
                    or Format.R5G6B5Unorm or Format.B5G6R5Unorm or Format.B5G5R5A1Unorm
                    or Format.A1B5G5R5Unorm or Format.B10G10R10A2Unorm => 4,
                Format.R16G16B16A16Float or Format.R16G16B16A16Unorm or Format.R16G16B16A16Snorm
                    or Format.R16G16B16A16Uint or Format.R16G16B16A16Sint
                    or Format.R16G16B16A16Uscaled or Format.R16G16B16A16Sscaled => 8,
                Format.R32G32B32A32Float or Format.R32G32B32A32Uint or Format.R32G32B32A32Sint
                    or Format.R32G32B32A32Uscaled or Format.R32G32B32A32Sscaled => 16,
                _ => 0
            };
        }
    }
}
