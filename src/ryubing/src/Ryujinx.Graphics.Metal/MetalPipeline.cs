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
        private const int MaxUniformBufferBindings = 31;
        private const int MaxStorageBufferBindings = 31;
        private const int MaxTextureBindings = 32;
        private const int MaxShaderStages = 3;

        private IProgram _program;
        private nint _pipelineHandle;
        private readonly nint _deviceHandle;
        private readonly nint _queueHandle;
        private readonly MetalBufferPool _buffers;
        private readonly VertexAttribDescriptor[] _vertexAttribs;
        private readonly VertexBufferDescriptor[] _vertexBuffers;
        private readonly MetalBufferBinding[] _uniformBuffers;
        private readonly MetalStorageBufferBinding[] _storageBuffers;
        private readonly MetalTextureBinding[,] _textureBindings;
        private PrimitiveTopology _primitiveTopology;
        private MetalIndexBufferBinding _indexBuffer;
        private int _vertexAttribCount;
        private int _vertexBufferCount;

        /// <summary>
        /// 当前活动的渲染管线句柄（由 metal_create_render_pipeline 返回）
        /// </summary>
        internal nint PipelineHandle => _pipelineHandle;

        public MetalPipeline(nint deviceHandle, nint queueHandle, MetalBufferPool buffers)
        {
            _deviceHandle = deviceHandle;
            _queueHandle = queueHandle;
            _buffers = buffers;
            _pipelineHandle = nint.Zero;
            _vertexAttribs = new VertexAttribDescriptor[MaxVertexAttributes];
            _vertexBuffers = new VertexBufferDescriptor[MaxVertexBufferBindings];
            _uniformBuffers = new MetalBufferBinding[MaxUniformBufferBindings];
            _storageBuffers = new MetalStorageBufferBinding[MaxStorageBufferBindings];
            _textureBindings = new MetalTextureBinding[MaxShaderStages, MaxTextureBindings];
            _primitiveTopology = PrimitiveTopology.Triangles;
            _indexBuffer = default;
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
            if (vertexCount <= 0 || instanceCount <= 0)
            {
                return;
            }

            ExecuteRenderDraw(renderEncoder =>
            {
                MetalResult result = MetalNative.RenderEncoderDrawPrimitives(
                    renderEncoder,
                    ConvertPrimitiveTopology(_primitiveTopology),
                    (uint)Math.Max(firstVertex, 0),
                    (uint)vertexCount,
                    (uint)instanceCount,
                    (uint)Math.Max(firstInstance, 0));
                ThrowIfFailed(result, nameof(MetalNative.RenderEncoderDrawPrimitives));
            });
        }

        public void DrawIndexed(int indexCount, int instanceCount, int firstIndex, int firstVertex, int firstInstance)
        {
            if (indexCount <= 0 || instanceCount <= 0)
            {
                return;
            }

            if (!TryGetIndexBufferBinding(out nint indexHandle, out ulong indexOffset, out ulong indexSize, out IndexType indexType))
            {
                return;
            }

            uint indexSizeBytes = indexType switch
            {
                IndexType.UShort => 2u,
                IndexType.UInt => 4u,
                _ => 0u,
            };

            if (indexSizeBytes == 0)
            {
                return;
            }

            ulong drawOffset = indexOffset + ((ulong)Math.Max(firstIndex, 0) * indexSizeBytes);
            ulong requiredBytes = (ulong)indexCount * indexSizeBytes;
            if (drawOffset < indexOffset || drawOffset + requiredBytes > indexOffset + indexSize)
            {
                return;
            }

            ExecuteRenderDraw(renderEncoder =>
            {
                MetalResult result = MetalNative.RenderEncoderDrawIndexedPrimitives(
                    renderEncoder,
                    ConvertPrimitiveTopology(_primitiveTopology),
                    (uint)indexCount,
                    ConvertIndexType(indexType),
                    indexHandle,
                    drawOffset,
                    (uint)instanceCount,
                    firstVertex,
                    (uint)Math.Max(firstInstance, 0));
                ThrowIfFailed(result, nameof(MetalNative.RenderEncoderDrawIndexedPrimitives));
            });
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
            if (buffer.Handle == BufferHandle.Null || buffer.Size <= 0)
            {
                _indexBuffer = default;
                return;
            }

            if (!_buffers.TryGet(buffer.Handle, out MetalBuffer metalBuffer))
            {
                _indexBuffer = default;
                return;
            }

            int safeOffset = Math.Clamp(buffer.Offset, 0, (int)metalBuffer.Size);
            int safeSize = Math.Clamp(buffer.Size, 0, (int)metalBuffer.Size - safeOffset);

            if (safeSize <= 0)
            {
                _indexBuffer = default;
                return;
            }

            _indexBuffer = new MetalIndexBufferBinding
            {
                Handle = metalBuffer.Handle,
                Offset = (ulong)safeOffset,
                Size = (ulong)safeSize,
                Type = type,
            };
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
            _primitiveTopology = topology;
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
            for (int i = 0; i < buffers.Length; i++)
            {
                BufferAssignment assignment = buffers[i];
                int binding = assignment.Binding;

                if ((uint)binding >= MaxStorageBufferBindings)
                {
                    continue;
                }

                BufferRange range = assignment.Range;

                if (range.Handle == BufferHandle.Null || range.Size <= 0)
                {
                    _storageBuffers[binding] = default;
                    continue;
                }

                if (!_buffers.TryGet(range.Handle, out MetalBuffer metalBuffer))
                {
                    _storageBuffers[binding] = default;
                    continue;
                }

                int safeOffset = Math.Clamp(range.Offset, 0, (int)metalBuffer.Size);
                int safeSize = Math.Clamp(range.Size, 0, (int)metalBuffer.Size - safeOffset);

                if (safeSize <= 0)
                {
                    _storageBuffers[binding] = default;
                    continue;
                }

                _storageBuffers[binding] = new MetalStorageBufferBinding
                {
                    Handle = metalBuffer.Handle,
                    Offset = (ulong)safeOffset,
                    Size = (ulong)safeSize,
                    Write = range.Write,
                };
            }
        }

        public void SetTextureAndSampler(ShaderStage stage, int binding, ITexture texture, ISampler sampler)
        {
            if (!TryGetShaderStageIndex(stage, out int stageIndex) ||
                (uint)binding >= MaxTextureBindings)
            {
                return;
            }

            nint textureHandle = nint.Zero;
            if (texture != null)
            {
                MetalTexture.TryGetNativeHandle(texture, out textureHandle);
            }

            nint samplerHandle = sampler is MetalSampler metalSampler
                ? metalSampler.Handle
                : nint.Zero;

            _textureBindings[stageIndex, binding] = new MetalTextureBinding
            {
                TextureHandle = textureHandle,
                SamplerHandle = samplerHandle,
            };
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
            for (int i = 0; i < buffers.Length; i++)
            {
                BufferAssignment assignment = buffers[i];
                int binding = assignment.Binding;

                if ((uint)binding >= MaxUniformBufferBindings)
                {
                    continue;
                }

                BufferRange range = assignment.Range;

                if (range.Handle == BufferHandle.Null || range.Size <= 0)
                {
                    _uniformBuffers[binding] = default;
                    continue;
                }

                if (!_buffers.TryGet(range.Handle, out MetalBuffer metalBuffer))
                {
                    _uniformBuffers[binding] = default;
                    continue;
                }

                int safeOffset = Math.Clamp(range.Offset, 0, (int)metalBuffer.Size);
                int safeSize = Math.Clamp(range.Size, 0, (int)metalBuffer.Size - safeOffset);

                if (safeSize <= 0)
                {
                    _uniformBuffers[binding] = default;
                    continue;
                }

                _uniformBuffers[binding] = new MetalBufferBinding
                {
                    Handle = metalBuffer.Handle,
                    Offset = (ulong)safeOffset,
                    Size = (ulong)safeSize,
                };
            }
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

        private void ExecuteRenderDraw(Action<nint> drawAction)
        {
            if (_pipelineHandle == nint.Zero || _queueHandle == nint.Zero || drawAction == null)
            {
                return;
            }

            MetalResult result = MetalNative.BeginCommandBuffer(_queueHandle, out nint commandBuffer);
            if (result != MetalResult.Ok || commandBuffer == nint.Zero)
            {
                ThrowIfFailed(result, nameof(MetalNative.BeginCommandBuffer));
                return;
            }

            nint renderEncoder = nint.Zero;

            try
            {
                result = MetalNative.BeginRenderEncoding(commandBuffer, _pipelineHandle, out renderEncoder);
                if (result != MetalResult.Ok || renderEncoder == nint.Zero)
                {
                    ThrowIfFailed(result, nameof(MetalNative.BeginRenderEncoding));
                    return;
                }

                BindRenderResources(renderEncoder);
                drawAction(renderEncoder);

                result = MetalNative.EndRenderEncoding(renderEncoder);
                ThrowIfFailed(result, nameof(MetalNative.EndRenderEncoding));

                result = MetalNative.CommitCommandBuffer(commandBuffer);
                ThrowIfFailed(result, nameof(MetalNative.CommitCommandBuffer));

                result = MetalNative.WaitCommandBuffer(commandBuffer);
                ThrowIfFailed(result, nameof(MetalNative.WaitCommandBuffer));
            }
            finally
            {
                if (renderEncoder != nint.Zero)
                {
                    MetalNative.Release(renderEncoder);
                }

                MetalNative.Release(commandBuffer);
            }
        }

        private void BindRenderResources(nint renderEncoder)
        {
            for (int binding = 0; binding < _vertexBufferCount; binding++)
            {
                if (!TryGetVertexBufferBinding(binding, out nint handle, out ulong offset, out _, out _, out _))
                {
                    continue;
                }

                MetalResult result = MetalNative.RenderEncoderSetVertexBuffer(
                    renderEncoder,
                    (uint)binding,
                    handle,
                    offset);
                ThrowIfFailed(result, nameof(MetalNative.RenderEncoderSetVertexBuffer));
            }

            for (int binding = 0; binding < MaxUniformBufferBindings; binding++)
            {
                if (!TryGetUniformBufferBinding(binding, out nint handle, out ulong offset, out _))
                {
                    continue;
                }

                MetalResult vertexResult = MetalNative.RenderEncoderSetVertexBuffer(
                    renderEncoder,
                    (uint)binding,
                    handle,
                    offset);
                ThrowIfFailed(vertexResult, nameof(MetalNative.RenderEncoderSetVertexBuffer));

                MetalResult fragmentResult = MetalNative.RenderEncoderSetFragmentBuffer(
                    renderEncoder,
                    (uint)binding,
                    handle,
                    offset);
                ThrowIfFailed(fragmentResult, nameof(MetalNative.RenderEncoderSetFragmentBuffer));
            }

            for (int binding = 0; binding < MaxStorageBufferBindings; binding++)
            {
                if (!TryGetStorageBufferBinding(binding, out nint handle, out ulong offset, out _, out _))
                {
                    continue;
                }

                MetalResult vertexResult = MetalNative.RenderEncoderSetVertexBuffer(
                    renderEncoder,
                    (uint)binding,
                    handle,
                    offset);
                ThrowIfFailed(vertexResult, nameof(MetalNative.RenderEncoderSetVertexBuffer));

                MetalResult fragmentResult = MetalNative.RenderEncoderSetFragmentBuffer(
                    renderEncoder,
                    (uint)binding,
                    handle,
                    offset);
                ThrowIfFailed(fragmentResult, nameof(MetalNative.RenderEncoderSetFragmentBuffer));
            }

            for (int binding = 0; binding < MaxTextureBindings; binding++)
            {
                if (!TryGetTextureBinding(ShaderStage.Fragment, binding, out nint textureHandle, out nint samplerHandle))
                {
                    continue;
                }

                if (textureHandle != nint.Zero)
                {
                    MetalResult textureResult = MetalNative.RenderEncoderSetFragmentTexture(
                        renderEncoder,
                        (uint)binding,
                        textureHandle);
                    ThrowIfFailed(textureResult, nameof(MetalNative.RenderEncoderSetFragmentTexture));
                }

                if (samplerHandle != nint.Zero)
                {
                    MetalResult samplerResult = MetalNative.RenderEncoderSetFragmentSampler(
                        renderEncoder,
                        (uint)binding,
                        samplerHandle);
                    ThrowIfFailed(samplerResult, nameof(MetalNative.RenderEncoderSetFragmentSampler));
                }
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

        private static MetalPrimitiveType ConvertPrimitiveTopology(PrimitiveTopology topology)
        {
            return topology switch
            {
                PrimitiveTopology.Points => MetalPrimitiveType.Point,
                PrimitiveTopology.Lines => MetalPrimitiveType.Line,
                PrimitiveTopology.LineStrip => MetalPrimitiveType.LineStrip,
                PrimitiveTopology.TriangleStrip => MetalPrimitiveType.TriangleStrip,
                _ => MetalPrimitiveType.Triangle,
            };
        }

        private static MetalIndexType ConvertIndexType(IndexType indexType)
        {
            return indexType switch
            {
                IndexType.UInt => MetalIndexType.UInt32,
                _ => MetalIndexType.UInt16,
            };
        }

        private static void ThrowIfFailed(MetalResult result, string operation)
        {
            if (result != MetalResult.Ok)
            {
                throw new InvalidOperationException($"{operation} 失败：{result}");
            }
        }

        /// <summary>
        /// 获取指定 binding 上当前缓存的 uniform buffer 绑定。
        /// 供后续 Draw/DrawIndexed 将状态批量下发到 render encoder 使用。
        /// </summary>
        internal bool TryGetUniformBufferBinding(int binding, out nint handle, out ulong offset, out ulong size)
        {
            if ((uint)binding >= MaxUniformBufferBindings)
            {
                handle = nint.Zero;
                offset = 0;
                size = 0;
                return false;
            }

            MetalBufferBinding bindingState = _uniformBuffers[binding];
            handle = bindingState.Handle;
            offset = bindingState.Offset;
            size = bindingState.Size;
            return handle != nint.Zero && size != 0;
        }

        internal bool TryGetTextureBinding(ShaderStage stage, int binding, out nint textureHandle, out nint samplerHandle)
        {
            if (!TryGetShaderStageIndex(stage, out int stageIndex) ||
                (uint)binding >= MaxTextureBindings)
            {
                textureHandle = nint.Zero;
                samplerHandle = nint.Zero;
                return false;
            }

            MetalTextureBinding bindingState = _textureBindings[stageIndex, binding];
            textureHandle = bindingState.TextureHandle;
            samplerHandle = bindingState.SamplerHandle;
            return textureHandle != nint.Zero || samplerHandle != nint.Zero;
        }

        internal PrimitiveTopology PrimitiveTopology => _primitiveTopology;

        internal bool TryGetIndexBufferBinding(out nint handle, out ulong offset, out ulong size, out IndexType type)
        {
            handle = _indexBuffer.Handle;
            offset = _indexBuffer.Offset;
            size = _indexBuffer.Size;
            type = _indexBuffer.Type;
            return handle != nint.Zero && size != 0;
        }

        internal bool TryGetVertexBufferBinding(int binding, out nint handle, out ulong offset, out ulong size, out uint stride, out uint stepRate)
        {
            if ((uint)binding >= _vertexBufferCount)
            {
                handle = nint.Zero;
                offset = 0;
                size = 0;
                stride = 0;
                stepRate = 0;
                return false;
            }

            VertexBufferDescriptor bufferDescriptor = _vertexBuffers[binding];
            BufferRange range = bufferDescriptor.Buffer;

            if (range.Handle == BufferHandle.Null || range.Size <= 0 || !_buffers.TryGet(range.Handle, out MetalBuffer metalBuffer))
            {
                handle = nint.Zero;
                offset = 0;
                size = 0;
                stride = 0;
                stepRate = 0;
                return false;
            }

            int safeOffset = Math.Clamp(range.Offset, 0, (int)metalBuffer.Size);
            int safeSize = Math.Clamp(range.Size, 0, (int)metalBuffer.Size - safeOffset);

            if (safeSize <= 0)
            {
                handle = nint.Zero;
                offset = 0;
                size = 0;
                stride = 0;
                stepRate = 0;
                return false;
            }

            handle = metalBuffer.Handle;
            offset = (ulong)safeOffset;
            size = (ulong)safeSize;
            stride = (uint)Math.Max(bufferDescriptor.Stride, 0);
            stepRate = (uint)Math.Max(bufferDescriptor.Divisor, 1);
            return true;
        }

        /// <summary>
        /// 获取指定 binding 上当前缓存的 storage buffer 绑定。
        /// 供后续 Draw/Dispatch 将状态批量下发到 render/compute encoder 使用。
        /// </summary>
        internal bool TryGetStorageBufferBinding(int binding, out nint handle, out ulong offset, out ulong size, out bool write)
        {
            if ((uint)binding >= MaxStorageBufferBindings)
            {
                handle = nint.Zero;
                offset = 0;
                size = 0;
                write = false;
                return false;
            }

            MetalStorageBufferBinding bindingState = _storageBuffers[binding];
            handle = bindingState.Handle;
            offset = bindingState.Offset;
            size = bindingState.Size;
            write = bindingState.Write;
            return handle != nint.Zero && size != 0;
        }

        private static bool TryGetShaderStageIndex(ShaderStage stage, out int stageIndex)
        {
            switch (stage)
            {
                case ShaderStage.Vertex:
                    stageIndex = 0;
                    return true;
                case ShaderStage.Fragment:
                    stageIndex = 1;
                    return true;
                case ShaderStage.Compute:
                    stageIndex = 2;
                    return true;
                default:
                    stageIndex = -1;
                    return false;
            }
        }

        private struct MetalBufferBinding
        {
            public nint Handle;
            public ulong Offset;
            public ulong Size;
        }

        private struct MetalTextureBinding
        {
            public nint TextureHandle;
            public nint SamplerHandle;
        }

        private struct MetalStorageBufferBinding
        {
            public nint Handle;
            public ulong Offset;
            public ulong Size;
            public bool Write;
        }

        private struct MetalIndexBufferBinding
        {
            public nint Handle;
            public ulong Offset;
            public ulong Size;
            public IndexType Type;
        }
    }
}
