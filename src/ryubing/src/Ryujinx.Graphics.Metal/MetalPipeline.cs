using Ryujinx.Graphics.GAL;
using Ryujinx.Graphics.Shader;
using System;
using System.Runtime.InteropServices;

namespace Ryujinx.Graphics.Metal
{
    internal sealed class MetalPipeline : IPipeline
    {
        private const int MaxVertexAttributes = 31;
        private const int MaxVertexBufferBindings = 31;

        private IProgram _program;
        private nint _pipelineHandle;
        private readonly nint _deviceHandle;
        private readonly VertexAttribDescriptor[] _vertexAttribs;
        private readonly VertexBufferDescriptor[] _vertexBuffers;
        private int _vertexAttribCount;
        private int _vertexBufferCount;

        /// <summary>
        /// 当前活动的渲染管线句柄（由 metal_create_render_pipeline 返回）
        /// </summary>
        internal nint PipelineHandle => _pipelineHandle;

        public MetalPipeline(nint deviceHandle)
        {
            _deviceHandle = deviceHandle;
            _pipelineHandle = nint.Zero;
            _vertexAttribs = new VertexAttribDescriptor[MaxVertexAttributes];
            _vertexBuffers = new VertexBufferDescriptor[MaxVertexBufferBindings];
        }

        public void Barrier()
        {
        }

        public void BeginTransformFeedback(PrimitiveTopology topology)
        {
        }

        public void ClearBuffer(BufferHandle destination, int offset, int size, uint value)
        {
        }

        public void ClearRenderTargetColor(int index, int layer, int layerCount, uint componentMask, ColorF color)
        {
        }

        public void ClearRenderTargetDepthStencil(int layer, int layerCount, float depthValue, bool depthMask, int stencilValue, int stencilMask)
        {
        }

        public void CommandBufferBarrier()
        {
        }

        public void CopyBuffer(BufferHandle source, BufferHandle destination, int srcOffset, int dstOffset, int size)
        {
        }

        public void DispatchCompute(int groupsX, int groupsY, int groupsZ)
        {
        }

        public void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
        {
        }

        public void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int firstVertex, int firstInstance)
        {
        }

        public void DrawIndexedIndirect(BufferRange indirectBuffer)
        {
        }

        public void DrawIndexedIndirectCount(BufferRange indirectBuffer, BufferRange parameterBuffer, int maxDrawCount, int stride)
        {
        }

        public void DrawIndirect(BufferRange indirectBuffer)
        {
        }

        public void DrawIndirectCount(BufferRange indirectBuffer, BufferRange parameterBuffer, int maxDrawCount, int stride)
        {
        }

        public void DrawTexture(ITexture texture, ISampler sampler, Extents2DF srcRegion, Extents2DF dstRegion)
        {
        }

        public void EndHostConditionalRendering()
        {
        }

        public void EndTransformFeedback()
        {
        }

        public void SetAlphaTest(bool enable, float reference, CompareOp op)
        {
        }

        public void SetBlendState(AdvancedBlendDescriptor blend)
        {
        }

        public void SetBlendState(int index, BlendDescriptor blend)
        {
        }

        public void SetDepthBias(PolygonModeMask enables, float factor, float units, float clamp)
        {
        }

        public void SetDepthClamp(bool clamp)
        {
        }

        public void SetDepthMode(DepthMode mode)
        {
        }

        public void SetDepthTest(DepthTestDescriptor depthTest)
        {
        }

        public void SetFaceCulling(bool enable, Face face)
        {
        }

        public void SetFrontFace(FrontFace frontFace)
        {
        }

        public void SetImage(ShaderStage stage, int binding, ITexture texture)
        {
        }

        public void SetImageArray(ShaderStage stage, int binding, IImageArray array)
        {
        }

        public void SetImageArraySeparate(ShaderStage stage, int setIndex, IImageArray array)
        {
        }

        public void SetIndexBuffer(BufferRange buffer, IndexType type)
        {
        }

        public void SetLineParameters(float width, bool smooth)
        {
        }

        public void SetLogicOpState(bool enable, LogicalOp op)
        {
        }

        public void SetMultisampleState(MultisampleDescriptor multisample)
        {
        }

        public void SetPatchParameters(int vertices, ReadOnlySpan<float> defaultOuterLevel, ReadOnlySpan<float> defaultInnerLevel)
        {
        }

        public void SetPointParameters(float size, bool isProgramPointSize, bool enablePointSprite, Origin origin)
        {
        }

        public void SetPolygonMode(PolygonMode frontMode, PolygonMode backMode)
        {
        }

        public void SetPrimitiveRestart(bool enable, int index)
        {
        }

        public void SetPrimitiveTopology(PrimitiveTopology topology)
        {
        }

        public void SetProgram(IProgram program)
        {
            if (ReferenceEquals(_program, program))
            {
                return;
            }

            // 释放旧的管线状态
            ReleasePipeline();

            _program = program;

            if (program == null)
            {
                return;
            }

            if (program is MetalProgram metalProgram)
            {
                CreatePipelineFromProgram(metalProgram);
            }
        }

        /// <summary>
        /// 从 MetalProgram 创建 MTLRenderPipelineState
        /// </summary>
        private void CreatePipelineFromProgram(MetalProgram program)
        {
            byte[] vertexMetallib = program.GetShaderMetallib(ShaderStage.Vertex);
            byte[] fragmentMetallib = program.GetShaderMetallib(ShaderStage.Fragment);

            if (vertexMetallib == null || vertexMetallib.Length == 0)
            {
                return;
            }

            // 固定 metallib 数据以传递指针到 native 层
            GCHandle vertexHandle = GCHandle.Alloc(vertexMetallib, GCHandleType.Pinned);
            GCHandle? fragmentHandle = null;
            if (fragmentMetallib != null && fragmentMetallib.Length > 0)
            {
                fragmentHandle = GCHandle.Alloc(fragmentMetallib, GCHandleType.Pinned);
            }

            try
            {
                var descriptor = new MetalRenderPipelineDescriptor
                {
                    AbiVersion = MetalNative.AbiVersion,
                    VertexMetallibData = vertexHandle.AddrOfPinnedObject(),
                    VertexMetallibSize = (ulong)vertexMetallib.Length,
                    FragmentMetallibData = fragmentHandle.HasValue
                        ? fragmentHandle.Value.AddrOfPinnedObject()
                        : nint.Zero,
                    FragmentMetallibSize = fragmentHandle.HasValue
                        ? (ulong)fragmentMetallib.Length
                        : 0UL,
                    VertexFunction = "main",
                    FragmentFunction = "main",
                    ColorAttachmentFormat = MetalPixelFormat.BGRA8Unorm,
                    DepthStencilFormat = MetalPixelFormat.Invalid,
                    VertexAttributeCount = 0,
                    VertexBufferLayoutCount = 0,
                    VertexAttributes = new MetalVertexAttributeDescriptor[MaxVertexAttributes],
                    VertexBufferLayouts = new MetalVertexBufferLayoutDescriptor[MaxVertexBufferBindings],
                    Reserved = new uint[2],
                };

                PopulateVertexLayout(ref descriptor);

                MetalResult result = MetalNative.CreateRenderPipeline(
                    _deviceHandle,
                    descriptor,
                    out nint pipelineHandle);

                if (result == MetalResult.Ok && pipelineHandle != nint.Zero)
                {
                    _pipelineHandle = pipelineHandle;
                }
                else
                {
                    Console.Error.WriteLine(
                        $"[MetalPipeline] CreateRenderPipeline 失败：{result}");
                }
            }
            finally
            {
                vertexHandle.Free();
                if (fragmentHandle.HasValue)
                {
                    fragmentHandle.Value.Free();
                }
            }
        }

        /// <summary>
        /// 释放当前管线状态句柄
        /// </summary>
        private void ReleasePipeline()
        {
            if (_pipelineHandle != nint.Zero)
            {
                MetalNative.Release(_pipelineHandle);
                _pipelineHandle = nint.Zero;
            }
        }

        public void SetRasterizerDiscard(bool discard)
        {
        }

        public void SetRenderTargetColorMasks(ReadOnlySpan<uint> componentMask)
        {
        }

        public void SetRenderTargets(Span<ITexture> colors, ITexture depthStencil)
        {
        }

        public void SetScissors(ReadOnlySpan<Rectangle<int>> regions)
        {
        }

        public void SetStencilTest(StencilTestDescriptor stencilTest)
        {
        }

        public void SetStorageBuffers(ReadOnlySpan<BufferAssignment> buffers)
        {
        }

        public void SetTextureAndSampler(ShaderStage stage, int binding, ITexture texture, ISampler sampler)
        {
        }

        public void SetTextureArray(ShaderStage stage, int binding, ITextureArray array)
        {
        }

        public void SetTextureArraySeparate(ShaderStage stage, int setIndex, ITextureArray array)
        {
        }

        public void SetTransformFeedbackBuffers(ReadOnlySpan<BufferRange> buffers)
        {
        }

        public void SetUniformBuffers(ReadOnlySpan<BufferAssignment> buffers)
        {
        }

        public void SetUserClipDistance(int index, bool enableClip)
        {
        }

        public void SetVertexAttribs(ReadOnlySpan<VertexAttribDescriptor> vertexAttribs)
        {
            int count = Math.Min(vertexAttribs.Length, MaxVertexAttributes);
            bool changed = _vertexAttribCount != count;

            for (int i = 0; i < count; i++)
            {
                VertexAttribDescriptor descriptor = vertexAttribs[i];
                if (!_vertexAttribs[i].Equals(descriptor))
                {
                    _vertexAttribs[i] = descriptor;
                    changed = true;
                }
            }

            for (int i = count; i < _vertexAttribCount; i++)
            {
                if (!_vertexAttribs[i].Equals(default))
                {
                    _vertexAttribs[i] = default;
                    changed = true;
                }
            }

            _vertexAttribCount = count;

            if (changed)
            {
                RecreatePipelineForLayoutChange();
            }
        }

        public void SetVertexBuffers(ReadOnlySpan<VertexBufferDescriptor> vertexBuffers)
        {
            int count = Math.Min(vertexBuffers.Length, MaxVertexBufferBindings);
            bool layoutChanged = _vertexBufferCount != count;

            for (int i = 0; i < count; i++)
            {
                VertexBufferDescriptor descriptor = vertexBuffers[i];

                if (_vertexBuffers[i].Stride != descriptor.Stride ||
                    _vertexBuffers[i].Divisor != descriptor.Divisor)
                {
                    layoutChanged = true;
                }

                _vertexBuffers[i] = descriptor;
            }

            for (int i = count; i < _vertexBufferCount; i++)
            {
                if (_vertexBuffers[i].Buffer.Handle != BufferHandle.Null ||
                    _vertexBuffers[i].Stride != 0 ||
                    _vertexBuffers[i].Divisor != 0)
                {
                    _vertexBuffers[i] = default;
                    layoutChanged = true;
                }
            }

            _vertexBufferCount = count;

            if (layoutChanged)
            {
                RecreatePipelineForLayoutChange();
            }
        }

        public void SetViewports(ReadOnlySpan<Viewport> viewports)
        {
        }

        public void TextureBarrier()
        {
        }

        public void TextureBarrierTiled()
        {
        }

        public bool TryHostConditionalRendering(ICounterEvent value, ulong compare, bool isEqual)
        {
            return false;
        }

        public bool TryHostConditionalRendering(ICounterEvent value, ICounterEvent compare, bool isEqual)
        {
            return false;
        }

        private void RecreatePipelineForLayoutChange()
        {
            if (_program is MetalProgram metalProgram)
            {
                ReleasePipeline();
                CreatePipelineFromProgram(metalProgram);
            }
        }

        private void PopulateVertexLayout(ref MetalRenderPipelineDescriptor descriptor)
        {
            int attrCount = _vertexAttribCount;
            if (attrCount > MaxVertexAttributes)
            {
                attrCount = MaxVertexAttributes;
            }

            int requiredBufferCount = _vertexBufferCount;

            for (int i = 0; i < attrCount; i++)
            {
                VertexAttribDescriptor attrib = _vertexAttribs[i];
                if (!TryConvertVertexFormat(attrib.Format, out MetalVertexFormat vertexFormat))
                {
                    continue;
                }

                int bufferIndex = attrib.BufferIndex;
                if ((uint)bufferIndex >= MaxVertexBufferBindings)
                {
                    continue;
                }

                requiredBufferCount = Math.Max(requiredBufferCount, bufferIndex + 1);

                descriptor.VertexAttributes[i] = new MetalVertexAttributeDescriptor
                {
                    AttributeIndex = (uint)i,
                    BufferIndex = (uint)bufferIndex,
                    Format = vertexFormat,
                    Offset = attrib.IsZero ? 0u : (uint)Math.Max(attrib.Offset, 0),
                };
            }

            descriptor.VertexAttributeCount = (uint)attrCount;

            int bufferCount = requiredBufferCount;
            if (bufferCount > MaxVertexBufferBindings)
            {
                bufferCount = MaxVertexBufferBindings;
            }

            for (int i = 0; i < bufferCount; i++)
            {
                VertexBufferDescriptor buffer = i < _vertexBufferCount ? _vertexBuffers[i] : default;
                descriptor.VertexBufferLayouts[i] = new MetalVertexBufferLayoutDescriptor
                {
                    BufferIndex = (uint)i,
                    Stride = (uint)Math.Max(buffer.Stride, 0),
                    StepFunction = i >= _vertexBufferCount
                        ? MetalVertexStepFunction.Constant
                        : buffer.Divisor != 0
                            ? MetalVertexStepFunction.PerInstance
                            : MetalVertexStepFunction.PerVertex,
                    StepRate = (uint)Math.Max(i < _vertexBufferCount ? buffer.Divisor : 1, 1),
                };
            }

            descriptor.VertexBufferLayoutCount = (uint)bufferCount;
        }

        private static bool TryConvertVertexFormat(Format format, out MetalVertexFormat metalFormat)
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
                Format.R8G8B8Unorm => MetalVertexFormat.UChar3Normalized,
                Format.R8G8B8Snorm => MetalVertexFormat.Char3Normalized,
                Format.R8G8B8Uint or Format.R8G8B8Uscaled => MetalVertexFormat.UChar3,
                Format.R8G8B8Sint or Format.R8G8B8Sscaled => MetalVertexFormat.Char3,
                Format.R16G16B16Float => MetalVertexFormat.Half3,
                Format.R16G16B16Unorm => MetalVertexFormat.UShort3Normalized,
                Format.R16G16B16Snorm => MetalVertexFormat.Short3Normalized,
                Format.R16G16B16Uint or Format.R16G16B16Uscaled => MetalVertexFormat.UShort3,
                Format.R16G16B16Sint or Format.R16G16B16Sscaled => MetalVertexFormat.Short3,
                Format.R32G32B32Float => MetalVertexFormat.Float3,
                Format.R32G32B32Uint or Format.R32G32B32Uscaled => MetalVertexFormat.UInt3,
                Format.R32G32B32Sint or Format.R32G32B32Sscaled => MetalVertexFormat.Int3,
                Format.R8G8B8A8Unorm or Format.R8G8B8A8Srgb => MetalVertexFormat.UChar4Normalized,
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
                Format.R11G11B10Float => MetalVertexFormat.FloatRg11B10,
                Format.R9G9B9E5Float => MetalVertexFormat.FloatRgb9E5,
                Format.B8G8R8A8Unorm or Format.B8G8R8A8Srgb => MetalVertexFormat.UChar4NormalizedBgra,
                _ => MetalVertexFormat.Invalid,
            };

            return metalFormat != MetalVertexFormat.Invalid;
        }
    }
}
