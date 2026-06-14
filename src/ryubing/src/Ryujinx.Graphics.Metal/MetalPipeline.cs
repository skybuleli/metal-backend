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
        private const int MaxViewports = 16;

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
        private readonly MetalRenderTargetState _renderTargets;
        private readonly MetalBlendAttachmentDescriptor[] _blendAttachments;
        private int _blendAttachmentCount;
        private nint _depthStencilStateHandle;
        private DepthTestDescriptor _depthTest;
        private StencilTestDescriptor _stencilTest;
        private bool _depthStencilDirty;
        private readonly MetalViewport[] _viewports;
        private int _viewportCount;
        private readonly MetalScissorRect[] _scissorRects;
        private int _scissorCount;
        private MetalCullMode _cullMode;
        private bool _cullEnabled;
        private MetalWinding _winding;
        private MetalTriangleFillMode _fillMode;
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
            _renderTargets = new MetalRenderTargetState();
            _blendAttachments = new MetalBlendAttachmentDescriptor[8];
            _blendAttachmentCount = 0;
            _depthStencilStateHandle = nint.Zero;
            _depthStencilDirty = false;
            _viewports = new MetalViewport[MaxViewports];
            _viewportCount = 0;
            _scissorRects = new MetalScissorRect[MaxViewports];
            _scissorCount = 0;
            _cullMode = MetalCullMode.None;
            _cullEnabled = false;
            _winding = MetalWinding.CounterClockwise;
            _fillMode = MetalTriangleFillMode.Fill;
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
            // 缓存颜色清除参数，下次 ExecuteRenderDraw 时合并到 MTLRenderPassDescriptor
            if (componentMask == 0 || (uint)index >= 8)
            {
                return;
            }

            _renderTargets.SetPendingColorClear(index, color, componentMask);
        }

        public void ClearRenderTargetDepthStencil(int layer, int layerCount, float depthValue, bool depthMask, int stencilValue, int stencilMask)
        {
            // 缓存深度/模板清除参数，下次 ExecuteRenderDraw 时合并到 MTLRenderPassDescriptor
            if (!depthMask && stencilMask == 0)
            {
                return;
            }

            _renderTargets.SetPendingDepthStencilClear(depthValue, depthMask, stencilValue, stencilMask);
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
            // Metal 不原生支持 KHR_blend_equation_advanced，回退为标准混合
            // 将高级混合操作转换为最接近的标准混合（Add + SrcAlpha/OneMinusSrcAlpha）
            for (int i = 0; i < 8; i++)
            {
                _blendAttachments[i] = new MetalBlendAttachmentDescriptor
                {
                    BlendingEnabled = (byte)(i == 0 ? 1 : 0),
                    ReservedPad = new byte[3],
                    SrcRgbFactor = MetalBlendFactor.SrcAlpha,
                    DstRgbFactor = MetalBlendFactor.OneMinusSrcAlpha,
                    RgbOperation = MetalBlendOperation.Add,
                    SrcAlphaFactor = MetalBlendFactor.One,
                    DstAlphaFactor = MetalBlendFactor.OneMinusSrcAlpha,
                    AlphaOperation = MetalBlendOperation.Add,
                    WriteMask = (uint)MetalColorWriteMask.All,
                };
            }

            _blendAttachmentCount = 8;
            RecreatePipelineForLayoutChange();
        }

        public void SetBlendState(int index, BlendDescriptor blend)
        {
            if ((uint)index >= 8)
            {
                return;
            }

            // 确保混合数组能覆盖到此索引
            int requiredCount = index + 1;
            if (requiredCount > _blendAttachmentCount)
            {
                // 初始化新增的槽位为默认（禁用混合）
                for (int i = _blendAttachmentCount; i < requiredCount; i++)
                {
                    _blendAttachments[i] = CreateDefaultBlendAttachment();
                }

                _blendAttachmentCount = requiredCount;
            }

            if (blend.Enable)
            {
                _blendAttachments[index] = new MetalBlendAttachmentDescriptor
                {
                    BlendingEnabled = 1,
                    ReservedPad = new byte[3],
                    SrcRgbFactor = ConvertBlendFactor(blend.ColorSrcFactor),
                    DstRgbFactor = ConvertBlendFactor(blend.ColorDstFactor),
                    RgbOperation = ConvertBlendOp(blend.ColorOp),
                    SrcAlphaFactor = ConvertBlendFactor(blend.AlphaSrcFactor),
                    DstAlphaFactor = ConvertBlendFactor(blend.AlphaDstFactor),
                    AlphaOperation = ConvertBlendOp(blend.AlphaOp),
                    WriteMask = (uint)MetalColorWriteMask.All,
                };
            }
            else
            {
                _blendAttachments[index] = CreateDefaultBlendAttachment();
            }

            RecreatePipelineForLayoutChange();
        }

        public void SetDepthBias(PolygonModeMask enables, float factor, float units, float clamp)
        {
        }

        public void SetDepthClamp(bool clamp)
        {
        }

        public void SetDepthMode(DepthMode mode)
        {
            // DepthMode 控制深度值范围（MinusOneToOne 或 ZeroToOne），
            // 通过管线描述符的 depthAttachmentPixelFormat 影响，
            // 当前默认 MinusOneToOne，后续扩展时处理
        }

        public void SetDepthTest(DepthTestDescriptor depthTest)
        {
            _depthTest = depthTest;
            _depthStencilDirty = true;
        }

        public void SetFaceCulling(bool enable, Face face)
        {
            _cullEnabled = enable;
            if (!enable)
            {
                _cullMode = MetalCullMode.None;
                return;
            }

            // GAL Face 值与 OpenGL 一致：Front=0x404, Back=0x405, FrontAndBack=0x408
            _cullMode = face switch
            {
                Face.Front => MetalCullMode.Front,
                Face.Back => MetalCullMode.Back,
                Face.FrontAndBack => MetalCullMode.None, // Metal 不支持同时剔除双面，回退为不剔除
                _ => MetalCullMode.None,
            };
        }

        public void SetFrontFace(FrontFace frontFace)
        {
            // GAL FrontFace 值与 OpenGL 一致：Clockwise=0x900, CounterClockwise=0x901
            // MTL::Winding: CounterClockwise=0, Clockwise=1
            _winding = frontFace switch
            {
                FrontFace.Clockwise => MetalWinding.Clockwise,
                FrontFace.CounterClockwise => MetalWinding.CounterClockwise,
                _ => MetalWinding.CounterClockwise,
            };
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
            // Metal 只有 setTriangleFillMode，不区分正反面；使用 frontMode
            // PolygonMode 值与 OpenGL 一致：Point=0x1b00, Line=0x1b01, Fill=0x1b02
            // Metal 不支持 Point 模式，回退为 Lines
            _fillMode = frontMode switch
            {
                PolygonMode.Fill => MetalTriangleFillMode.Fill,
                PolygonMode.Line => MetalTriangleFillMode.Lines,
                PolygonMode.Point => MetalTriangleFillMode.Lines, // 回退
                _ => MetalTriangleFillMode.Fill,
            };
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
            GCHandle? blendHandle = null;
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
                    BlendAttachments = nint.Zero,
                    BlendAttachmentCount = 0,
                    Reserved = 0,
                };

                PopulateVertexLayout(ref descriptor);

                // 固定混合附件数组并设置指针（P4.3.9）
                if (_blendAttachmentCount > 0)
                {
                    blendHandle = GCHandle.Alloc(_blendAttachments, GCHandleType.Pinned);
                    descriptor.BlendAttachments = blendHandle.Value.AddrOfPinnedObject();
                    descriptor.BlendAttachmentCount = (uint)_blendAttachmentCount;
                }

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

                if (blendHandle.HasValue)
                {
                    blendHandle.Value.Free();
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
            _renderTargets.Set(colors, depthStencil);
        }

        public void SetScissors(ReadOnlySpan<Rectangle<int>> regions)
        {
            int count = Math.Min(MaxViewports, regions.Length);
            for (int i = 0; i < count; i++)
            {
                Rectangle<int> r = regions[i];
                _scissorRects[i] = new MetalScissorRect
                {
                    X = (uint)Math.Max(0, r.X),
                    Y = (uint)Math.Max(0, r.Y),
                    Width = (uint)Math.Max(0, r.Width),
                    Height = (uint)Math.Max(0, r.Height),
                };
            }
            _scissorCount = count;
        }

        public void SetStencilTest(StencilTestDescriptor stencilTest)
        {
            _stencilTest = stencilTest;
            _depthStencilDirty = true;
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
            int count = Math.Min(MaxViewports, viewports.Length);
            for (int i = 0; i < count; i++)
            {
                Viewport vp = viewports[i];
                float width = vp.Region.Width == 0f ? 1f : vp.Region.Width;
                float height = vp.Region.Height == 0f ? 1f : vp.Region.Height;

                // Vulkan 视口 Y 轴向下，Metal Y 轴向上；通过翻转 originY 进行坐标转换
                // originY = |height| - region.Y - region.Height
                double originY = Math.Abs(height) - vp.Region.Y - height;

                _viewports[i] = new MetalViewport
                {
                    OriginX = vp.Region.X,
                    OriginY = originY,
                    Width = width,
                    Height = Math.Abs(height),
                    ZNear = Math.Clamp(vp.DepthNear, 0f, 1f),
                    ZFar = Math.Clamp(vp.DepthFar, 0f, 1f),
                };
            }
            _viewportCount = count;
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
                if (_renderTargets.HasTargets)
                {
                    // 使用真实的渲染目标创建 render encoding
                    MetalColorAttachmentDescriptor[] colorDescs = _renderTargets.BuildColorDescriptors();
                    uint colorCount = (uint)_renderTargets.ColorCount;

                    if (_renderTargets.HasDepthStencil)
                    {
                        MetalDepthStencilAttachmentDescriptor dsDesc = _renderTargets.BuildDepthStencilDescriptor();
                        result = MetalNative.BeginRenderEncodingWithTargets(
                            commandBuffer, _pipelineHandle,
                            colorDescs, colorCount,
                            dsDesc, out renderEncoder);
                    }
                    else
                    {
                        result = MetalNative.BeginRenderEncodingWithTargets(
                            commandBuffer, _pipelineHandle,
                            colorDescs, colorCount,
                            out renderEncoder);
                    }
                }
                else
                {
                    // 无渲染目标时使用内部临时 1x1 附件（P4.3.6 回退路径）
                    result = MetalNative.BeginRenderEncoding(
                        commandBuffer, _pipelineHandle, out renderEncoder);
                }

                if (result != MetalResult.Ok || renderEncoder == nint.Zero)
                {
                    ThrowIfFailed(result, nameof(MetalNative.BeginRenderEncoding));
                    return;
                }

                // 渲染通道开始后立即清除挂起的清除请求
                // （清除参数已合并到描述符的 loadAction=Clear 中）
                _renderTargets.ClearPending();

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
            // 深度/模板状态更新与绑定（P4.3.10）
            UpdateDepthStencilState();
            if (_depthStencilStateHandle != nint.Zero)
            {
                MetalResult dsResult = MetalNative.RenderEncoderSetDepthStencilState(
                    renderEncoder, _depthStencilStateHandle);
                ThrowIfFailed(dsResult, nameof(MetalNative.RenderEncoderSetDepthStencilState));

                // 设置模板引用值
                if (_stencilTest.TestEnable)
                {
                    MetalResult srResult = MetalNative.RenderEncoderSetStencilReferenceValue(
                        renderEncoder,
                        (uint)_stencilTest.FrontFuncRef,
                        (uint)_stencilTest.BackFuncRef);
                    ThrowIfFailed(srResult, nameof(MetalNative.RenderEncoderSetStencilReferenceValue));
                }
            }
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

            // 设置视口（P4.3.11）
            if (_viewportCount > 0)
            {
                unsafe
                {
                    fixed (MetalViewport* vpPtr = _viewports)
                    {
                        MetalResult vpResult = MetalNative.RenderEncoderSetViewports(
                            renderEncoder, (nint)vpPtr, (uint)_viewportCount);
                        ThrowIfFailed(vpResult, nameof(MetalNative.RenderEncoderSetViewports));
                    }
                }
            }

            // 设置裁剪矩形（P4.3.11）
            if (_scissorCount > 0)
            {
                unsafe
                {
                    fixed (MetalScissorRect* srPtr = _scissorRects)
                    {
                        MetalResult srResult = MetalNative.RenderEncoderSetScissorRects(
                            renderEncoder, (nint)srPtr, (uint)_scissorCount);
                        ThrowIfFailed(srResult, nameof(MetalNative.RenderEncoderSetScissorRects));
                    }
                }
            }

            // 设置面剔除模式（P4.3.12）
            MetalResult cullResult = MetalNative.RenderEncoderSetCullMode(
                renderEncoder, _cullEnabled ? _cullMode : MetalCullMode.None);
            ThrowIfFailed(cullResult, nameof(MetalNative.RenderEncoderSetCullMode));

            // 设置正面绕线方向（P4.3.12）
            MetalResult windingResult = MetalNative.RenderEncoderSetFrontFacingWinding(
                renderEncoder, _winding);
            ThrowIfFailed(windingResult, nameof(MetalNative.RenderEncoderSetFrontFacingWinding));

            // 设置三角形填充模式（P4.3.12）
            MetalResult fillResult = MetalNative.RenderEncoderSetTriangleFillMode(
                renderEncoder, _fillMode);
            ThrowIfFailed(fillResult, nameof(MetalNative.RenderEncoderSetTriangleFillMode));

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

        /// <summary>
        /// 更新深度/模板状态对象（P4.3.10）。
        /// 仅当 _depthStencilDirty 时重新创建 MTLDepthStencilState。
        /// </summary>
        private void UpdateDepthStencilState()
        {
            if (!_depthStencilDirty)
            {
                return;
            }

            // 释放旧的深度/模板状态
            if (_depthStencilStateHandle != nint.Zero)
            {
                MetalNative.Release(_depthStencilStateHandle);
                _depthStencilStateHandle = nint.Zero;
            }

            bool depthEnabled = _depthTest.TestEnable;
            bool stencilEnabled = _stencilTest.TestEnable;

            if (!depthEnabled && !stencilEnabled)
            {
                _depthStencilDirty = false;
                return;
            }

            var descriptor = new MetalDepthStencilDescriptor
            {
                DepthCompareFunction = ConvertCompareOp(_depthTest.Func),
                DepthWriteEnabled = (byte)(_depthTest.WriteEnable ? 1 : 0),
                StencilEnabled = (byte)(stencilEnabled ? 1 : 0),
                ReservedPad = new byte[2],
                FrontFace = new MetalStencilDescriptor
                {
                    CompareFunction = ConvertCompareOp(_stencilTest.FrontFunc),
                    StencilFailure = ConvertStencilOp(_stencilTest.FrontSFail),
                    DepthFailure = ConvertStencilOp(_stencilTest.FrontDpFail),
                    DepthStencilPass = ConvertStencilOp(_stencilTest.FrontDpPass),
                    ReadMask = (uint)_stencilTest.FrontFuncMask,
                    WriteMask = (uint)_stencilTest.FrontMask,
                },
                BackFace = new MetalStencilDescriptor
                {
                    CompareFunction = ConvertCompareOp(_stencilTest.BackFunc),
                    StencilFailure = ConvertStencilOp(_stencilTest.BackSFail),
                    DepthFailure = ConvertStencilOp(_stencilTest.BackDpFail),
                    DepthStencilPass = ConvertStencilOp(_stencilTest.BackDpPass),
                    ReadMask = (uint)_stencilTest.BackFuncMask,
                    WriteMask = (uint)_stencilTest.BackMask,
                },
            };

            // 深度测试未启用时使用 Always 比较 + 禁用写入
            if (!depthEnabled)
            {
                descriptor.DepthCompareFunction = MetalCompareFunction.Always;
                descriptor.DepthWriteEnabled = 0;
            }

            MetalResult result = MetalNative.CreateDepthStencilState(
                _deviceHandle, descriptor, out nint stateHandle);

            if (result == MetalResult.Ok && stateHandle != nint.Zero)
            {
                _depthStencilStateHandle = stateHandle;
            }
            else
            {
                Console.Error.WriteLine(
                    $"[MetalPipeline] CreateDepthStencilState 失败：{result}");
            }

            _depthStencilDirty = false;
        }

        /// <summary>
        /// 将 GAL CompareOp 转换为 Metal 比较函数（P4.3.10）。
        /// </summary>
        private static MetalCompareFunction ConvertCompareOp(CompareOp op)
        {
            return op switch
            {
                CompareOp.Never or CompareOp.NeverGl => MetalCompareFunction.Never,
                CompareOp.Less or CompareOp.LessGl => MetalCompareFunction.Less,
                CompareOp.Equal or CompareOp.EqualGl => MetalCompareFunction.Equal,
                CompareOp.LessOrEqual or CompareOp.LessOrEqualGl => MetalCompareFunction.LessEqual,
                CompareOp.Greater or CompareOp.GreaterGl => MetalCompareFunction.Greater,
                CompareOp.NotEqual or CompareOp.NotEqualGl => MetalCompareFunction.NotEqual,
                CompareOp.GreaterOrEqual or CompareOp.GreaterOrEqualGl => MetalCompareFunction.GreaterEqual,
                CompareOp.Always or CompareOp.AlwaysGl => MetalCompareFunction.Always,
                _ => MetalCompareFunction.Always,
            };
        }

        /// <summary>
        /// 将 GAL StencilOp 转换为 Metal 模板操作（P4.3.10）。
        /// </summary>
        private static MetalStencilOperation ConvertStencilOp(StencilOp op)
        {
            return op switch
            {
                StencilOp.Keep or StencilOp.KeepGl => MetalStencilOperation.Keep,
                StencilOp.Zero or StencilOp.ZeroGl => MetalStencilOperation.Zero,
                StencilOp.Replace or StencilOp.ReplaceGl => MetalStencilOperation.Replace,
                StencilOp.IncrementAndClamp or StencilOp.IncrementAndClampGl => MetalStencilOperation.IncrementClamp,
                StencilOp.DecrementAndClamp or StencilOp.DecrementAndClampGl => MetalStencilOperation.DecrementClamp,
                StencilOp.Invert or StencilOp.InvertGl => MetalStencilOperation.Invert,
                StencilOp.IncrementAndWrap or StencilOp.IncrementAndWrapGl => MetalStencilOperation.IncrementWrap,
                StencilOp.DecrementAndWrap or StencilOp.DecrementAndWrapGl => MetalStencilOperation.DecrementWrap,
                _ => MetalStencilOperation.Keep,
            };
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

        /// <summary>
        /// 将 GAL BlendFactor 转换为 Metal 混合因子（P4.3.9）。
        /// 参考：Metal Shading Language Specification - Table 5.3 Blend Factors
        /// </summary>
        private static MetalBlendFactor ConvertBlendFactor(BlendFactor factor)
        {
            return factor switch
            {
                BlendFactor.Zero or BlendFactor.ZeroGl => MetalBlendFactor.Zero,
                BlendFactor.One or BlendFactor.OneGl => MetalBlendFactor.One,
                BlendFactor.SrcColor or BlendFactor.SrcColorGl => MetalBlendFactor.SrcColor,
                BlendFactor.OneMinusSrcColor or BlendFactor.OneMinusSrcColorGl => MetalBlendFactor.OneMinusSrcColor,
                BlendFactor.SrcAlpha or BlendFactor.SrcAlphaGl => MetalBlendFactor.SrcAlpha,
                BlendFactor.OneMinusSrcAlpha or BlendFactor.OneMinusSrcAlphaGl => MetalBlendFactor.OneMinusSrcAlpha,
                BlendFactor.DstAlpha or BlendFactor.DstAlphaGl => MetalBlendFactor.DstAlpha,
                BlendFactor.OneMinusDstAlpha or BlendFactor.OneMinusDstAlphaGl => MetalBlendFactor.OneMinusDstAlpha,
                BlendFactor.DstColor or BlendFactor.DstColorGl => MetalBlendFactor.DstColor,
                BlendFactor.OneMinusDstColor or BlendFactor.OneMinusDstColorGl => MetalBlendFactor.OneMinusDstColor,
                BlendFactor.SrcAlphaSaturate or BlendFactor.SrcAlphaSaturateGl => MetalBlendFactor.SrcAlphaSaturate,
                BlendFactor.ConstantColor => MetalBlendFactor.BlendColor,
                BlendFactor.OneMinusConstantColor => MetalBlendFactor.OneMinusBlendColor,
                BlendFactor.ConstantAlpha => MetalBlendFactor.BlendAlpha,
                BlendFactor.OneMinusConstantAlpha => MetalBlendFactor.OneMinusBlendAlpha,
                BlendFactor.Src1Color or BlendFactor.Src1ColorGl => MetalBlendFactor.Src1Color,
                BlendFactor.OneMinusSrc1Color or BlendFactor.OneMinusSrc1ColorGl => MetalBlendFactor.OneMinusSrc1Color,
                BlendFactor.Src1Alpha or BlendFactor.Src1AlphaGl => MetalBlendFactor.Src1Alpha,
                BlendFactor.OneMinusSrc1Alpha or BlendFactor.OneMinusSrc1AlphaGl => MetalBlendFactor.OneMinusSrc1Alpha,
                _ => MetalBlendFactor.Zero,
            };
        }

        /// <summary>
        /// 将 GAL BlendOp 转换为 Metal 混合操作（P4.3.9）。
        /// </summary>
        private static MetalBlendOperation ConvertBlendOp(BlendOp op)
        {
            return op switch
            {
                BlendOp.Add or BlendOp.AddGl => MetalBlendOperation.Add,
                BlendOp.Subtract or BlendOp.SubtractGl => MetalBlendOperation.Subtract,
                BlendOp.ReverseSubtract or BlendOp.ReverseSubtractGl => MetalBlendOperation.ReverseSubtract,
                BlendOp.Minimum or BlendOp.MinimumGl => MetalBlendOperation.Min,
                BlendOp.Maximum or BlendOp.MaximumGl => MetalBlendOperation.Max,
                _ => MetalBlendOperation.Add,
            };
        }

        /// <summary>
        /// 创建默认的混合附件描述符（禁用混合，全写入）。
        /// </summary>
        private static MetalBlendAttachmentDescriptor CreateDefaultBlendAttachment()
        {
            return new MetalBlendAttachmentDescriptor
            {
                BlendingEnabled = 0,
                ReservedPad = new byte[3],
                SrcRgbFactor = MetalBlendFactor.One,
                DstRgbFactor = MetalBlendFactor.Zero,
                RgbOperation = MetalBlendOperation.Add,
                SrcAlphaFactor = MetalBlendFactor.One,
                DstAlphaFactor = MetalBlendFactor.Zero,
                AlphaOperation = MetalBlendOperation.Add,
                WriteMask = (uint)MetalColorWriteMask.All,
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

        /// <summary>
        /// 渲染目标状态缓存（P4.3.7 / P4.3.8）。
        /// 由 SetRenderTargets 更新，ClearRenderTarget* 缓存清除参数，
        /// ExecuteRenderDraw 读取以创建带清除动作的 MTLRenderPassDescriptor。
        /// </summary>
        private sealed class MetalRenderTargetState
        {
            private const int MaxColorAttachments = 8;

            private readonly nint[] _colorHandles;
            private int _colorCount;
            private nint _depthStencilHandle;

            // P4.3.8：挂起的清除参数
            private readonly PendingColorClear[] _pendingColorClears;
            private PendingDepthStencilClear _pendingDepthClear;

            public int ColorCount => _colorCount;
            public bool HasTargets => _colorCount > 0;
            public bool HasDepthStencil => _depthStencilHandle != nint.Zero;

            public MetalRenderTargetState()
            {
                _colorHandles = new nint[MaxColorAttachments];
                _colorCount = 0;
                _depthStencilHandle = nint.Zero;
                _pendingColorClears = new PendingColorClear[MaxColorAttachments];
                _pendingDepthClear = default;
            }

            /// <summary>
            /// 缓存指定颜色附件的清除参数（P4.3.8）。
            /// componentMask 为 0xF 时使用渲染通道描述符的全通道清除；
            /// 部分清除当前回退为全通道清除（Metal 不支持按通道清除）。
            /// </summary>
            public void SetPendingColorClear(int index, ColorF color, uint componentMask)
            {
                if ((uint)index >= MaxColorAttachments)
                {
                    return;
                }

                _pendingColorClears[index] = new PendingColorClear
                {
                    Active = true,
                    Color = color,
                    ComponentMask = componentMask,
                };
            }

            /// <summary>
            /// 缓存深度/模板清除参数（P4.3.8）。
            /// </summary>
            public void SetPendingDepthStencilClear(float depth, bool depthMask, int stencil, int stencilMask)
            {
                _pendingDepthClear = new PendingDepthStencilClear
                {
                    Active = true,
                    Depth = depth,
                    DepthMask = depthMask,
                    Stencil = stencil,
                    StencilMask = stencilMask,
                };
            }

            /// <summary>
            /// 渲染通道开始后调用，重置所有挂起的清除请求。
            /// </summary>
            public void ClearPending()
            {
                for (int i = 0; i < MaxColorAttachments; i++)
                {
                    _pendingColorClears[i] = default;
                }

                _pendingDepthClear = default;
            }

            /// <summary>
            /// 从 GAL 接口更新渲染目标状态。
            /// 提取 ITexture 中的原生 Metal 纹理句柄以供后续 P/Invoke 使用。
            /// </summary>
            public void Set(Span<ITexture> colors, ITexture depthStencil)
            {
                int count = Math.Min(colors.Length, MaxColorAttachments);
                int validCount = 0;

                for (int i = 0; i < count; i++)
                {
                    ITexture texture = colors[i];
                    if (texture != null && MetalTexture.TryGetNativeHandle(texture, out nint handle))
                    {
                        _colorHandles[i] = handle;
                        validCount++;
                    }
                    else
                    {
                        _colorHandles[i] = nint.Zero;
                    }
                }

                // 清空剩余的槽位
                for (int i = count; i < MaxColorAttachments; i++)
                {
                    _colorHandles[i] = nint.Zero;
                }

                _colorCount = validCount;

                // 解析深度/模板纹理句柄
                if (depthStencil != null && MetalTexture.TryGetNativeHandle(depthStencil, out nint dsHandle))
                {
                    _depthStencilHandle = dsHandle;
                }
                else
                {
                    _depthStencilHandle = nint.Zero;
                }
            }

            /// <summary>
            /// 构建 C ABI 颜色附件描述符数组（P4.3.7 / P4.3.8）。
            /// 默认使用 LoadAction::Load + StoreAction::Store 保留已有内容。
            /// 如果有挂起的清除请求，对应附件使用 LoadAction::Clear。
            /// </summary>
            public MetalColorAttachmentDescriptor[] BuildColorDescriptors()
            {
                var descriptors = new MetalColorAttachmentDescriptor[MaxColorAttachments];

                for (int i = 0; i < MaxColorAttachments; i++)
                {
                    if (_colorHandles[i] != nint.Zero)
                    {
                        PendingColorClear pending = _pendingColorClears[i];

                        if (pending.Active)
                        {
                            // Metal 渲染通道清除作用于所有分量，忽略 componentMask 的部分清除语义
                            descriptors[i] = new MetalColorAttachmentDescriptor
                            {
                                Texture = _colorHandles[i],
                                Level = 0,
                                Slice = 0,
                                LoadAction = MetalLoadAction.Clear,
                                StoreAction = MetalStoreAction.Store,
                                ClearColor = new MetalClearColor
                                {
                                    Red = pending.Color.Red,
                                    Green = pending.Color.Green,
                                    Blue = pending.Color.Blue,
                                    Alpha = pending.Color.Alpha,
                                },
                            };
                        }
                        else
                        {
                            descriptors[i] = new MetalColorAttachmentDescriptor
                            {
                                Texture = _colorHandles[i],
                                Level = 0,
                                Slice = 0,
                                LoadAction = MetalLoadAction.Load,
                                StoreAction = MetalStoreAction.Store,
                                ClearColor = default,
                            };
                        }
                    }
                    else
                    {
                        descriptors[i] = default;
                    }
                }

                return descriptors;
            }

            /// <summary>
            /// 构建 C ABI 深度/模板附件描述符（P4.3.7 / P4.3.8）。
            /// 如果有挂起的深度/模板清除请求，使用对应的 Clear load action。
            /// </summary>
            public MetalDepthStencilAttachmentDescriptor BuildDepthStencilDescriptor()
            {
                PendingDepthStencilClear pending = _pendingDepthClear;

                if (pending.Active)
                {
                    // 根据 depthMask 和 stencilMask 决定各通道的 load action
                    MetalLoadAction depthLoad = pending.DepthMask
                        ? MetalLoadAction.Clear
                        : MetalLoadAction.Load;
                    MetalLoadAction stencilLoad = (pending.StencilMask != 0)
                        ? MetalLoadAction.Clear
                        : MetalLoadAction.Load;

                    return new MetalDepthStencilAttachmentDescriptor
                    {
                        Texture = _depthStencilHandle,
                        Level = 0,
                        Slice = 0,
                        DepthLoadAction = depthLoad,
                        DepthStoreAction = MetalStoreAction.Store,
                        StencilLoadAction = stencilLoad,
                        StencilStoreAction = MetalStoreAction.Store,
                        ClearValue = new MetalClearDepthStencil
                        {
                            Depth = pending.Depth,
                            Stencil = (uint)pending.Stencil,
                        },
                    };
                }

                return new MetalDepthStencilAttachmentDescriptor
                {
                    Texture = _depthStencilHandle,
                    Level = 0,
                    Slice = 0,
                    DepthLoadAction = MetalLoadAction.Load,
                    DepthStoreAction = MetalStoreAction.Store,
                    StencilLoadAction = MetalLoadAction.Load,
                    StencilStoreAction = MetalStoreAction.Store,
                    ClearValue = default,
                };
            }
        }

        /// <summary>
        /// 挂起的颜色清除参数（P4.3.8）
        /// </summary>
        private struct PendingColorClear
        {
            public bool Active;
            public ColorF Color;
            public uint ComponentMask;
        }

        /// <summary>
        /// 挂起的深度/模板清除参数（P4.3.8）
        /// </summary>
        private struct PendingDepthStencilClear
        {
            public bool Active;
            public float Depth;
            public bool DepthMask;
            public int Stencil;
            public int StencilMask;
        }
    }
}
