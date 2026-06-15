using Ryujinx.Graphics.GAL;
using Ryujinx.Graphics.Shader;
using System;
using Ryujinx.Common.Logging;

using System.Runtime.InteropServices;

using System.Diagnostics;
using System.Collections.Generic;

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
        private const int ReservedVertexBufferSlots = 3;
        private const int ZeroVertexBufferIndex = ReservedVertexBufferSlots;
        private const int FirstUserVertexBufferIndex = ZeroVertexBufferIndex + 1;
        private const uint DefaultVertexStride = 16;

        // 诊断计数器
        private ulong _diagnosticDrawCount;
        private ulong _diagnosticDrawTextureCount;
        private long _diagnosticLastLogTicks;
        private static readonly long DiagnosticLogIntervalTicks = Stopwatch.Frequency * 5; // 5秒

        private IProgram _program;
        private nint _pipelineHandle;
        private nint _computePipelineHandle;
        private readonly nint _deviceHandle;
        private readonly nint _queueHandle;
        private readonly MetalBufferPool _buffers;
        private readonly VertexAttribDescriptor[] _vertexAttribs;
        private readonly VertexBufferDescriptor[] _vertexBuffers;
        private readonly MetalBufferBinding[] _uniformBuffers;
        private readonly MetalStorageBufferBinding[] _storageBuffers;
        private readonly MetalTextureBinding[,] _textureBindings;
        private readonly int[,] _textureArrayLengths;
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
        private uint _patchControlPoints;
        private MetalIndexBufferBinding _indexBuffer;
        private BufferRange[] _transformFeedbackBuffers = Array.Empty<BufferRange>();
        private bool _transformFeedbackActive;
        private bool _rasterizerDiscard;
        private PolygonModeMask _depthBiasEnables;
        private float _depthBiasFactor;
        private float _depthBiasUnits;
        private float _depthBiasClamp;
        private bool _depthClamp;
        private int _vertexAttribCount;
        private int _vertexBufferCount;
        private readonly nint _zeroVertexBufferHandle;
        private MetalPixelFormat _pipelineColorFormat;
        private MetalPixelFormat _pipelineDepthStencilFormat;
        private MetalProgram _currentProgram;
        private readonly Dictionary<(MetalPixelFormat, MetalPixelFormat), nint> _pipelineCache = new();
        private readonly HashSet<string> _loggedFormatCombos = new();
        private int _renderWidth;
        private int _renderHeight;
        // 帧级 CommandBuffer 批处理：减少每 Draw 创建/提交开销
        private nint _currentCommandBuffer;
        private nint _currentRenderEncoder;
        private nint _currentComputeEncoder;
        private bool _encoderActive;
        private bool _computeEncoderActive;
        private bool _renderTargetsChanged;

        /// <summary>
        /// 当前活动的渲染管线句柄（由 metal_create_render_pipeline 返回）
        /// </summary>
        internal nint PipelineHandle => _pipelineHandle;

        internal uint PatchControlPoints => _patchControlPoints;

        public MetalPipeline(nint deviceHandle, nint queueHandle, MetalBufferPool buffers)
        {
            _deviceHandle = deviceHandle;
            _queueHandle = queueHandle;
            _buffers = buffers;
            _pipelineHandle = nint.Zero;
            _computePipelineHandle = nint.Zero;
            _vertexAttribs = new VertexAttribDescriptor[MaxVertexAttributes];
            _vertexBuffers = new VertexBufferDescriptor[MaxVertexBufferBindings];
            _uniformBuffers = new MetalBufferBinding[MaxUniformBufferBindings];
            _storageBuffers = new MetalStorageBufferBinding[MaxStorageBufferBindings];
            _textureBindings = new MetalTextureBinding[MaxShaderStages, MaxTextureBindings];
            _textureArrayLengths = new int[MaxShaderStages, MaxTextureBindings];
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
            _zeroVertexBufferHandle = CreateZeroVertexBuffer();
            _pipelineColorFormat = MetalPixelFormat.BGRA8Unorm;
            _pipelineDepthStencilFormat = MetalPixelFormat.Invalid;
            _renderWidth = 1920;
            _renderHeight = 1080;

        }

        public void Barrier()
        {
        }

        public void BeginTransformFeedback(PrimitiveTopology topology)
        {
            _primitiveTopology = topology;
            _transformFeedbackActive = true;
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
            if (size <= 0)
            {
                return;
            }

            if (_queueHandle == nint.Zero)
            {
                return;
            }

            if (!_buffers.TryGet(source, out MetalBuffer sourceBuffer) ||
                !_buffers.TryGet(destination, out MetalBuffer destinationBuffer))
            {
                return;
            }

            int safeSrcOffset = Math.Clamp(srcOffset, 0, (int)sourceBuffer.Size);
            int safeDstOffset = Math.Clamp(dstOffset, 0, (int)destinationBuffer.Size);
            int safeSize = Math.Min(size, Math.Min((int)sourceBuffer.Size - safeSrcOffset, (int)destinationBuffer.Size - safeDstOffset));

            if (safeSize <= 0)
            {
                return;
            }

            if (_encoderActive || _computeEncoderActive)
            {
                Flush();
            }

            MetalResult result = MetalNative.CopyBuffer(
                _queueHandle,
                sourceBuffer.Handle,
                destinationBuffer.Handle,
                (ulong)safeSrcOffset,
                (ulong)safeDstOffset,
                (ulong)safeSize);

            ThrowIfFailed(result, nameof(MetalNative.CopyBuffer));
        }

        public void DispatchCompute(int groupsX, int groupsY, int groupsZ)
        {
            if (groupsX <= 0 || groupsY <= 0 || groupsZ <= 0)
            {
                return;
            }

            if (_currentProgram == null || _computePipelineHandle == nint.Zero || _queueHandle == nint.Zero)
            {
                return;
            }

            if (_encoderActive)
            {
                Flush();
            }

            try
            {
                if (!_computeEncoderActive)
                {
                    MetalResult result = MetalNative.BeginCommandBuffer(
                        _queueHandle, out _currentCommandBuffer);
                    if (result != MetalResult.Ok || _currentCommandBuffer == nint.Zero)
                    {
                        ThrowIfFailed(result, nameof(MetalNative.BeginCommandBuffer));
                        return;
                    }

                    result = MetalNative.BeginComputeEncoding(
                        _currentCommandBuffer,
                        _computePipelineHandle,
                        out _currentComputeEncoder);
                    if (result != MetalResult.Ok || _currentComputeEncoder == nint.Zero)
                    {
                        Flush();
                        ThrowIfFailed(result, nameof(MetalNative.BeginComputeEncoding));
                        return;
                    }

                    _computeEncoderActive = true;
                }

                BindComputeResources(_currentComputeEncoder);

                if (!_currentProgram.TryGetComputeLocalSize(out int threadX, out int threadY, out int threadZ))
                {
                    threadX = 1;
                    threadY = 1;
                    threadZ = 1;
                }

                MetalResult dispatchResult = MetalNative.ComputeEncoderDispatchThreadgroups(
                    _currentComputeEncoder,
                    (uint)groupsX,
                    (uint)groupsY,
                    (uint)groupsZ,
                    (uint)threadX,
                    (uint)threadY,
                    (uint)threadZ);

                ThrowIfFailed(dispatchResult, nameof(MetalNative.ComputeEncoderDispatchThreadgroups));
            }
            catch
            {
                Flush();
                throw;
            }
        }

        public void Draw(int vertexCount, int instanceCount, int firstVertex, int firstInstance)
        {
            if (vertexCount <= 0 || instanceCount <= 0)
            {
                return;
            }

            _diagnosticDrawCount++;
            LogDiagnosticIfNeeded();

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

            _diagnosticDrawCount++;
            LogDiagnosticIfNeeded();

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
            if (!TryGetIndexBufferBinding(out nint indexHandle, out ulong indexOffset, out _, out IndexType indexType))
            {
                return;
            }

            if (!TryGetIndirectBuffer(indirectBuffer, out nint indirectHandle, out ulong indirectOffset))
            {
                return;
            }

            _diagnosticDrawCount++;
            LogDiagnosticIfNeeded();

            ExecuteRenderDraw(renderEncoder =>
            {
                MetalResult result = MetalNative.RenderEncoderDrawIndexedPrimitivesIndirect(
                    renderEncoder,
                    ConvertPrimitiveTopology(_primitiveTopology),
                    ConvertIndexType(indexType),
                    indexHandle,
                    indexOffset,
                    indirectHandle,
                    indirectOffset);
                ThrowIfFailed(result, nameof(MetalNative.RenderEncoderDrawIndexedPrimitivesIndirect));
            });
        }

        public void DrawIndexedIndirectCount(BufferRange indirectBuffer, BufferRange parameterBuffer, int maxDrawCount, int stride)
        {
            if (maxDrawCount <= 0 || stride <= 0)
            {
                return;
            }

            if (!TryGetIndexBufferBinding(out nint indexHandle, out ulong indexOffset, out _, out IndexType indexType))
            {
                return;
            }

            if (!TryGetIndirectBuffer(indirectBuffer, out nint indirectHandle, out ulong indirectOffset))
            {
                return;
            }

            uint drawCount = Math.Min((uint)maxDrawCount, ReadIndirectDrawCount(parameterBuffer));
            if (drawCount == 0)
            {
                return;
            }

            _diagnosticDrawCount += drawCount;
            LogDiagnosticIfNeeded();

            ExecuteRenderDraw(renderEncoder =>
            {
                for (uint i = 0; i < drawCount; i++)
                {
                    MetalResult result = MetalNative.RenderEncoderDrawIndexedPrimitivesIndirect(
                        renderEncoder,
                        ConvertPrimitiveTopology(_primitiveTopology),
                        ConvertIndexType(indexType),
                        indexHandle,
                        indexOffset,
                        indirectHandle,
                        indirectOffset + (ulong)i * (ulong)stride);
                    ThrowIfFailed(result, nameof(MetalNative.RenderEncoderDrawIndexedPrimitivesIndirect));
                }
            });
        }

        public void DrawIndirect(BufferRange indirectBuffer)
        {
            if (!TryGetIndirectBuffer(indirectBuffer, out nint indirectHandle, out ulong indirectOffset))
            {
                return;
            }

            _diagnosticDrawCount++;
            LogDiagnosticIfNeeded();

            ExecuteRenderDraw(renderEncoder =>
            {
                MetalResult result = MetalNative.RenderEncoderDrawPrimitivesIndirect(
                    renderEncoder,
                    ConvertPrimitiveTopology(_primitiveTopology),
                    indirectHandle,
                    indirectOffset);
                ThrowIfFailed(result, nameof(MetalNative.RenderEncoderDrawPrimitivesIndirect));
            });
        }

        public void DrawIndirectCount(BufferRange indirectBuffer, BufferRange parameterBuffer, int maxDrawCount, int stride)
        {
            if (maxDrawCount <= 0 || stride <= 0)
            {
                return;
            }

            if (!TryGetIndirectBuffer(indirectBuffer, out nint indirectHandle, out ulong indirectOffset))
            {
                return;
            }

            uint drawCount = Math.Min((uint)maxDrawCount, ReadIndirectDrawCount(parameterBuffer));
            if (drawCount == 0)
            {
                return;
            }

            _diagnosticDrawCount += drawCount;
            LogDiagnosticIfNeeded();

            ExecuteRenderDraw(renderEncoder =>
            {
                for (uint i = 0; i < drawCount; i++)
                {
                    MetalResult result = MetalNative.RenderEncoderDrawPrimitivesIndirect(
                        renderEncoder,
                        ConvertPrimitiveTopology(_primitiveTopology),
                        indirectHandle,
                        indirectOffset + (ulong)i * (ulong)stride);
                    ThrowIfFailed(result, nameof(MetalNative.RenderEncoderDrawPrimitivesIndirect));
                }
            });
        }

        public void DrawTexture(ITexture texture, ISampler sampler, Extents2DF srcRegion, Extents2DF dstRegion)
        {
            _diagnosticDrawTextureCount++;

            if (_diagnosticDrawTextureCount <= 5 || (_diagnosticDrawTextureCount % 100) == 0)
            {
                string textureFormat = texture != null && MetalTexture.TryGetMetalFormat(texture, out MetalPixelFormat format)
                    ? format.ToString()
                    : "Unknown";

                Logger.Warning?.PrintMsg(
                    LogClass.Gpu,
                    $"[DIAG] DrawTexture 仍为 stub: count={_diagnosticDrawTextureCount}, textureFormat={textureFormat}, src=({srcRegion.X1:F1},{srcRegion.Y1:F1})-({srcRegion.X2:F1},{srcRegion.Y2:F1}), dst=({dstRegion.X1:F1},{dstRegion.Y1:F1})-({dstRegion.X2:F1},{dstRegion.Y2:F1})");
            }
        }

        public void EndHostConditionalRendering()
        {
        }

        public void EndTransformFeedback()
        {
            _transformFeedbackActive = false;
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
                _blendAttachments[i] = MetalBlendStateMapping.CreateFallbackAdvancedAttachment(i == 0);
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
                    _blendAttachments[i] = MetalBlendStateMapping.CreateDisabledAttachment();
                }

                _blendAttachmentCount = requiredCount;
            }

            if (blend.Enable)
            {
                _blendAttachments[index] = MetalBlendStateMapping.CreateAttachment(blend);
            }
            else
            {
                _blendAttachments[index] = MetalBlendStateMapping.CreateDisabledAttachment();
            }

            RecreatePipelineForLayoutChange();
        }

        public void SetDepthBias(PolygonModeMask enables, float factor, float units, float clamp)
        {
            _depthBiasEnables = enables;
            _depthBiasFactor = factor;
            _depthBiasUnits = units;
            _depthBiasClamp = clamp;
        }

        public void SetDepthClamp(bool clamp)
        {
            _depthClamp = clamp;
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
            _cullMode = MetalRasterizerStateMapping.ToMetalCullMode(enable, face);
        }

        public void SetFrontFace(FrontFace frontFace)
        {
            _winding = MetalRasterizerStateMapping.ToMetalWinding(frontFace);
        }

        public void SetImage(ShaderStage stage, int binding, ITexture texture)
        {
            if (!TryGetShaderStageIndex(stage, out int stageIndex) ||
                (uint)binding >= MaxTextureBindings)
            {
                return;
            }

            ClearTextureArrayBinding(stageIndex, binding);

            nint textureHandle = nint.Zero;
            if (texture != null)
            {
                MetalTexture.TryGetNativeHandle(texture, out textureHandle);
            }

            _textureBindings[stageIndex, binding] = new MetalTextureBinding
            {
                TextureHandle = textureHandle,
                SamplerHandle = nint.Zero,
            };
        }

        public void SetImageArray(ShaderStage stage, int binding, IImageArray array)
        {
            if (!TryGetShaderStageIndex(stage, out int stageIndex) ||
                (uint)binding >= MaxTextureBindings)
            {
                return;
            }

            ClearTextureArrayBinding(stageIndex, binding);

            if (array is not MetalImageArray metalImageArray)
            {
                return;
            }

            BindTextureArray(stageIndex, binding, metalImageArray.Images, ReadOnlySpan<ISampler>.Empty);
        }

        public void SetImageArraySeparate(ShaderStage stage, int setIndex, IImageArray array)
        {
            SetImageArray(stage, setIndex, array);
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
            _patchControlPoints = (uint)Math.Max(vertices, 0);
            // Metal 没有原生 tessellation pipeline，这里先缓存 patch 状态供后续
            // compute 化的 Post-TCS 路径和诊断信息使用。
        }

        public void SetPointParameters(float size, bool isProgramPointSize, bool enablePointSprite, Origin origin)
        {
        }

        public void SetPolygonMode(PolygonMode frontMode, PolygonMode backMode)
        {
            _fillMode = MetalRasterizerStateMapping.ToMetalFillMode(frontMode);
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

            if (_encoderActive || _computeEncoderActive)
            {
                Flush();
            }

            ReleaseComputePipeline();

            // 程序变更：释放所有缓存的管线
            ReleaseAllPipelines();
            _program = program;

            if (program == null)
            {
                return;
            }

            if (program is MetalProgram metalProgram)
            {
                _currentProgram = metalProgram;
                _pipelineHandle = GetOrCreatePipeline(metalProgram, _pipelineColorFormat, _pipelineDepthStencilFormat);
                _computePipelineHandle = GetOrCreateComputePipeline(metalProgram);
            }
            else
            {
                _currentProgram = null;
                _computePipelineHandle = nint.Zero;
            }

            if (_encoderActive)
            {
                _renderTargetsChanged = true;
            }
        }

        /// <summary>
        /// 从 MetalProgram + 格式创建 MTLRenderPipelineState，返回句柄。
        /// 由 SetProgram 或 SetRenderTargets 调用，并通过缓存复用。
        /// </summary>
        private nint CreatePipelineForFormat(MetalProgram program, MetalPixelFormat colorFormat, MetalPixelFormat depthStencilFormat)
        {
            byte[] vertexMetallib = program.GetShaderMetallib(ShaderStage.Vertex);
            byte[] fragmentMetallib = program.GetShaderMetallib(ShaderStage.Fragment);

            if (vertexMetallib == null || vertexMetallib.Length == 0)
            {
                return nint.Zero;
            }

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
                    ColorAttachmentFormat = colorFormat,
                    DepthStencilFormat = depthStencilFormat,
                    VertexAttributeCount = 0,
                    VertexBufferLayoutCount = 0,
                    VertexAttributes = new MetalVertexAttributeDescriptor[MaxVertexAttributes],
                    VertexBufferLayouts = new MetalVertexBufferLayoutDescriptor[MaxVertexBufferBindings],
                    BlendAttachments = nint.Zero,
                    BlendAttachmentCount = 0,
                    Reserved = 0,
                };

                MetalVertexDescriptorMapping.PopulateVertexLayout(
                    ref descriptor,
                    _vertexAttribs.AsSpan(0, _vertexAttribCount),
                    _vertexBuffers.AsSpan(0, _vertexBufferCount),
                    MaxVertexAttributes,
                    MaxVertexBufferBindings,
                    ZeroVertexBufferIndex,
                    FirstUserVertexBufferIndex,
                    DefaultVertexStride);

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

                if (result != MetalResult.Ok || pipelineHandle == nint.Zero)
                {
                    Console.Error.WriteLine(
                        $"[MetalPipeline] CreateRenderPipeline 失败：{result}");
                    return nint.Zero;
                }

                return pipelineHandle;
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
        /// 获取或创建管线，按格式缓存。
        /// </summary>
        private nint GetOrCreatePipeline(MetalProgram program, MetalPixelFormat colorFormat, MetalPixelFormat depthStencilFormat)
        {
            var key = (colorFormat, depthStencilFormat);
            if (_pipelineCache.TryGetValue(key, out nint cached))
            {
                return cached;
            }

            nint handle = CreatePipelineForFormat(program, colorFormat, depthStencilFormat);
            if (handle != nint.Zero)
            {
                _pipelineCache[key] = handle;
            }
            return handle;
        }

        /// <summary>
        /// 从 MetalProgram 创建 compute pipeline，compute 不依赖颜色格式。
        /// </summary>
        private nint GetOrCreateComputePipeline(MetalProgram program)
        {
            byte[] computeMetallib = program.GetShaderMetallib(ShaderStage.Compute);
            if (computeMetallib == null || computeMetallib.Length == 0)
            {
                return nint.Zero;
            }

            GCHandle computeHandle = GCHandle.Alloc(computeMetallib, GCHandleType.Pinned);
            try
            {
                MetalComputePipelineDescriptor descriptor = new()
                {
                    AbiVersion = MetalNative.AbiVersion,
                    MetallibData = computeHandle.AddrOfPinnedObject(),
                    MetallibSize = (ulong)computeMetallib.Length,
                    FunctionName = "main",
                    Reserved = 0,
                };

                MetalResult result = MetalNative.CreateComputePipeline(
                    _deviceHandle,
                    descriptor,
                    out nint pipelineHandle);

                if (result != MetalResult.Ok || pipelineHandle == nint.Zero)
                {
                    Console.Error.WriteLine(
                        $"[MetalPipeline] CreateComputePipeline 失败：{result}");
                    return nint.Zero;
                }

                return pipelineHandle;
            }
            finally
            {
                computeHandle.Free();
            }
        }

        /// <summary>
        /// 释放单个管线句柄
        /// </summary>
        private void ReleasePipelineHandle(nint handle)
        {
            if (handle != nint.Zero)
            {
                MetalNative.Release(handle);
            }
        }

        private void ReleaseComputePipeline()
        {
            if (_computePipelineHandle != nint.Zero)
            {
                MetalNative.Release(_computePipelineHandle);
                _computePipelineHandle = nint.Zero;
            }
        }

        /// <summary>
        /// 释放当前活动的管线句柄
        /// </summary>
        private void ReleaseActivePipeline()
        {
            if (_pipelineHandle != nint.Zero)
            {
                MetalNative.Release(_pipelineHandle);
                _pipelineHandle = nint.Zero;
            }

            if (_computePipelineHandle != nint.Zero)
            {
                MetalNative.Release(_computePipelineHandle);
                _computePipelineHandle = nint.Zero;
            }
        }

        /// <summary>
        /// 释放所有缓存的管线（程序变更时调用）
        /// </summary>
        private void ReleaseAllPipelines()
        {
            foreach (var kv in _pipelineCache)
            {
                if (kv.Value != nint.Zero)
                {
                    MetalNative.Release(kv.Value);
                }
            }
            _pipelineCache.Clear();
            _pipelineHandle = nint.Zero;
        }

        public void SetRasterizerDiscard(bool discard)
        {
            _rasterizerDiscard = discard;
        }

        public void SetRenderTargetColorMasks(ReadOnlySpan<uint> componentMask)
        {
        }

        public void SetRenderTargets(Span<ITexture> colors, ITexture depthStencil)
        {
            bool formatsChanged = _renderTargets.Set(colors, depthStencil, out MetalPixelFormat colorFormat, out MetalPixelFormat depthStencilFormat);

            // 跟踪渲染目标维度用于 scissor 裁剪
            if (colors.Length > 0 && colors[0] != null)
            {
                _renderWidth = colors[0].Width;
                _renderHeight = colors[0].Height;
            }

            if (formatsChanged && _currentProgram != null)
            {
                _pipelineColorFormat = colorFormat;
                _pipelineDepthStencilFormat = depthStencilFormat;

                // 从缓存获取管线，首次访问时创建
                _pipelineHandle = GetOrCreatePipeline(_currentProgram, colorFormat, depthStencilFormat);

                // 调试日志：仅首次出现的格式组合记录
                string formatKey = $"color={colorFormat},depth={depthStencilFormat}";
                if (_loggedFormatCombos.Add(formatKey) && _loggedFormatCombos.Count <= 10)
                {
                    Logger.Info?.PrintMsg(LogClass.Gpu, $"[DIAG] Pipeline 缓存: {formatKey}");
                }
            }

            if (formatsChanged && _encoderActive)
            {
                _renderTargetsChanged = true;
            }
        }

        public void SetScissors(ReadOnlySpan<Rectangle<int>> regions)
        {
            int count = Math.Min(MaxViewports, regions.Length);
            for (int i = 0; i < count; i++)
            {
                _scissorRects[i] = MetalViewportScissorMapping.ToMetalScissorRect(
                    regions[i],
                    _renderWidth,
                    _renderHeight);
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

            ClearTextureArrayBinding(stageIndex, binding);

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
            if (!TryGetShaderStageIndex(stage, out int stageIndex) ||
                (uint)binding >= MaxTextureBindings)
            {
                return;
            }

            ClearTextureArrayBinding(stageIndex, binding);

            if (array is not MetalTextureArray metalTextureArray)
            {
                return;
            }

            BindTextureArray(stageIndex, binding, metalTextureArray.Textures, metalTextureArray.Samplers);
        }

        public void SetTextureArraySeparate(ShaderStage stage, int setIndex, ITextureArray array)
        {
            SetTextureArray(stage, setIndex, array);
        }

        public void SetTransformFeedbackBuffers(ReadOnlySpan<BufferRange> buffers)
        {
            int count = buffers.Length;

            if (_transformFeedbackBuffers.Length != count)
            {
                _transformFeedbackBuffers = new BufferRange[count];
            }

            buffers.CopyTo(_transformFeedbackBuffers);

            if (_transformFeedbackActive && count > 0)
            {
                Logger.Warning?.PrintMsg(LogClass.Gpu,
                    $"[DIAG] TransformFeedback buffers 已缓存: count={count}, topology={_primitiveTopology}");
            }
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
                _viewports[i] = MetalViewportScissorMapping.ToMetalViewport(viewports[i]);
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
            Logger.Warning?.PrintMsg(LogClass.Gpu, "[DIAG] Metal 暂未接通 host conditional rendering，回退到 CPU 路径。");
            return false;
        }

        public bool TryHostConditionalRendering(ICounterEvent value, ICounterEvent compare, bool isEqual)
        {
            Logger.Warning?.PrintMsg(LogClass.Gpu, "[DIAG] Metal 暂未接通 host conditional rendering，回退到 CPU 路径。");
            return false;
        }

        private bool TryGetIndirectBuffer(BufferRange indirectBuffer, out nint indirectHandle, out ulong indirectOffset)
        {
            indirectHandle = nint.Zero;
            indirectOffset = 0;

            if (indirectBuffer.Handle == BufferHandle.Null || indirectBuffer.Size <= 0)
            {
                return false;
            }

            if (!_buffers.TryGet(indirectBuffer.Handle, out MetalBuffer metalBuffer))
            {
                return false;
            }

            if (indirectBuffer.Offset < 0 || indirectBuffer.Offset >= (long)metalBuffer.Size)
            {
                return false;
            }

            indirectHandle = metalBuffer.Handle;
            indirectOffset = (ulong)indirectBuffer.Offset;
            return true;
        }

        private uint ReadIndirectDrawCount(BufferRange parameterBuffer)
        {
            if (parameterBuffer.Handle == BufferHandle.Null || parameterBuffer.Size < sizeof(uint))
            {
                return 0;
            }

            using PinnedSpan<byte> pinned = _buffers.GetData(parameterBuffer.Handle, parameterBuffer.Offset, sizeof(uint));
            ReadOnlySpan<byte> bytes = pinned.Get();

            if (bytes.Length < sizeof(uint))
            {
                return 0;
            }

            return MemoryMarshal.Read<uint>(bytes);
        }

        private void RecreatePipelineForLayoutChange()
        {
            if (_currentProgram != null)
            {
                ReleaseAllPipelines();
                _pipelineHandle = GetOrCreatePipeline(_currentProgram, _pipelineColorFormat, _pipelineDepthStencilFormat);
            }
        }

        private void LogDiagnosticIfNeeded()
        {
            long now = Stopwatch.GetTimestamp();
            if (now - _diagnosticLastLogTicks >= DiagnosticLogIntervalTicks)
            {
                _diagnosticLastLogTicks = now;
                double seconds = (double)now / Stopwatch.Frequency;
                Logger.Info?.PrintMsg(LogClass.Gpu, $"[DIAG] Draw 总数: {_diagnosticDrawCount}, 运行时间: {seconds:F1}s");
            }
        }

        private void ExecuteRenderDraw(Action<nint> drawAction)
        {
            if (_rasterizerDiscard || _pipelineHandle == nint.Zero || _queueHandle == nint.Zero || drawAction == null)
            {
                return;
            }

            try
            {
                if (_computeEncoderActive)
                {
                    Flush();
                }

                // 当 encoder 已激活但有待处理的清除操作或渲染目标变更时，需重建 encoder
                if (_encoderActive && (_renderTargetsChanged || _renderTargets.HasPending()))
                {
                    Flush();
                }

                // 延迟创建 CommandBuffer + RenderEncoder
                if (!_encoderActive)
                {
                    MetalResult result = MetalNative.BeginCommandBuffer(
                        _queueHandle, out _currentCommandBuffer);
                    if (result != MetalResult.Ok || _currentCommandBuffer == nint.Zero)
                    {
                        ThrowIfFailed(result, nameof(MetalNative.BeginCommandBuffer));
                        return;
                    }

                    if (_renderTargets.HasTargets)
                    {
                        MetalColorAttachmentDescriptor[] colorDescs = _renderTargets.BuildColorDescriptors();
                        uint colorCount = (uint)_renderTargets.ColorCount;

                        if (_renderTargets.HasDepthStencil)
                        {
                            MetalDepthStencilAttachmentDescriptor dsDesc = _renderTargets.BuildDepthStencilDescriptor();
                            result = MetalNative.BeginRenderEncodingWithTargets(
                                _currentCommandBuffer, _pipelineHandle,
                                colorDescs, colorCount,
                                dsDesc, out _currentRenderEncoder);
                        }
                        else
                        {
                            result = MetalNative.BeginRenderEncodingWithTargets(
                                _currentCommandBuffer, _pipelineHandle,
                                colorDescs, colorCount,
                                out _currentRenderEncoder);
                        }
                    }
                    else
                    {
                        result = MetalNative.BeginRenderEncoding(
                            _currentCommandBuffer, _pipelineHandle,
                            out _currentRenderEncoder);
                    }

                    if (result != MetalResult.Ok || _currentRenderEncoder == nint.Zero)
                    {
                        ThrowIfFailed(result, nameof(MetalNative.BeginRenderEncoding));
                        Flush(); // 清理已创建的 CB
                        return;
                    }

                    _renderTargets.ClearPending();
                    _renderTargetsChanged = false;
                    _encoderActive = true;
                }

                // 绑定渲染资源和绘制
                BindRenderResources(_currentRenderEncoder);
                drawAction(_currentRenderEncoder);
            }
            catch
            {
                // 异常时清理，避免悬挂
                Flush();
                throw;
            }

            // _encoderActive 保持 true，下次 Draw 复用 encoder
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
                    (uint)(binding + FirstUserVertexBufferIndex),
                    handle,
                    offset);
                ThrowIfFailed(result, nameof(MetalNative.RenderEncoderSetVertexBuffer));
            }

            if (UsesZeroVertexAttributes() && _zeroVertexBufferHandle != nint.Zero)
            {
                MetalResult zeroResult = MetalNative.RenderEncoderSetVertexBuffer(
                    renderEncoder,
                    ZeroVertexBufferIndex,
                    _zeroVertexBufferHandle,
                    0);
                ThrowIfFailed(zeroResult, nameof(MetalNative.RenderEncoderSetVertexBuffer));
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

        private void BindComputeResources(nint computeEncoder)
        {
            for (int binding = 0; binding < MaxUniformBufferBindings; binding++)
            {
                if (!TryGetUniformBufferBinding(binding, out nint handle, out ulong offset, out _))
                {
                    continue;
                }

                MetalResult result = MetalNative.ComputeEncoderSetBuffer(
                    computeEncoder,
                    (uint)binding,
                    handle,
                    offset);
                ThrowIfFailed(result, nameof(MetalNative.ComputeEncoderSetBuffer));
            }

            for (int binding = 0; binding < MaxStorageBufferBindings; binding++)
            {
                if (!TryGetStorageBufferBinding(binding, out nint handle, out ulong offset, out _, out _))
                {
                    continue;
                }

                MetalResult result = MetalNative.ComputeEncoderSetBuffer(
                    computeEncoder,
                    (uint)binding,
                    handle,
                    offset);
                ThrowIfFailed(result, nameof(MetalNative.ComputeEncoderSetBuffer));
            }

            for (int binding = 0; binding < MaxTextureBindings; binding++)
            {
                if (!TryGetTextureBinding(ShaderStage.Compute, binding, out nint textureHandle, out nint samplerHandle))
                {
                    continue;
                }

                if (textureHandle != nint.Zero)
                {
                    MetalResult textureResult = MetalNative.ComputeEncoderSetTexture(
                        computeEncoder,
                        (uint)binding,
                        textureHandle);
                    ThrowIfFailed(textureResult, nameof(MetalNative.ComputeEncoderSetTexture));
                }

                if (samplerHandle != nint.Zero)
                {
                    MetalResult samplerResult = MetalNative.ComputeEncoderSetSampler(
                        computeEncoder,
                        (uint)binding,
                        samplerHandle);
                    ThrowIfFailed(samplerResult, nameof(MetalNative.ComputeEncoderSetSampler));
                }
            }
        }

        /// 提交当前帧的 CommandBuffer：结束编码器 + 提交命令缓冲。
        /// 由 Present 或渲染目标变更时调用。
        /// </summary>
        public void Flush()
        {
            if (_currentRenderEncoder != nint.Zero)
            {
                MetalResult endResult = MetalNative.EndRenderEncoding(_currentRenderEncoder);
                _ = endResult; // 静默忽略错误（后续 Commands 已提交）
                MetalNative.Release(_currentRenderEncoder);
                _currentRenderEncoder = nint.Zero;
            }

            if (_currentComputeEncoder != nint.Zero)
            {
                MetalResult endResult = MetalNative.EndComputeEncoding(_currentComputeEncoder);
                _ = endResult;
                MetalNative.Release(_currentComputeEncoder);
                _currentComputeEncoder = nint.Zero;
            }

            if (_currentCommandBuffer != nint.Zero)
            {
                MetalResult commitResult = MetalNative.CommitCommandBuffer(_currentCommandBuffer);
                _ = commitResult;
                MetalNative.Release(_currentCommandBuffer);
                _currentCommandBuffer = nint.Zero;
            }

            _encoderActive = false;
            _computeEncoderActive = false;
        }

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

            if (!_depthTest.TestEnable && !_stencilTest.TestEnable)
            {
                _depthStencilDirty = false;
                return;
            }

            MetalDepthStencilDescriptor descriptor = MetalDepthStencilStateMapping.CreateDescriptor(_depthTest, _stencilTest);

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

        private bool UsesZeroVertexAttributes()
        {
            for (int i = 0; i < _vertexAttribCount; i++)
            {
                if (_vertexAttribs[i].IsZero)
                {
                    return true;
                }
            }

            return false;
        }

        private nint CreateZeroVertexBuffer()
        {
            BufferHandle zeroBuffer = _buffers.Create((int)DefaultVertexStride, BufferAccess.Default);
            if (zeroBuffer == BufferHandle.Null || !_buffers.TryGet(zeroBuffer, out MetalBuffer metalBuffer))
            {
                return nint.Zero;
            }

            if (MetalNative.MapBuffer(metalBuffer.Handle, out nint ptr) == MetalResult.Ok)
            {
                try
                {
                    for (int i = 0; i < (int)Math.Min(metalBuffer.Size, DefaultVertexStride); i++)
                    {
                        Marshal.WriteByte(ptr, i, 0);
                    }
                }
                finally
                {
                    MetalNative.UnmapBuffer(metalBuffer.Handle);
                }
            }

            return metalBuffer.Handle;
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

        private void BindTextureArray(
            int stageIndex,
            int binding,
            ReadOnlySpan<ITexture> textures,
            ReadOnlySpan<ISampler> samplers)
        {
            if ((uint)stageIndex >= MaxShaderStages || (uint)binding >= MaxTextureBindings)
            {
                return;
            }

            int maxCount = Math.Min(textures.Length, MaxTextureBindings - binding);
            _textureArrayLengths[stageIndex, binding] = maxCount;

            for (int i = 0; i < maxCount; i++)
            {
                nint textureHandle = nint.Zero;
                ITexture texture = textures[i];
                if (texture != null)
                {
                    MetalTexture.TryGetNativeHandle(texture, out textureHandle);
                }

                nint samplerHandle = nint.Zero;
                if (i < samplers.Length && samplers[i] is MetalSampler metalSampler)
                {
                    samplerHandle = metalSampler.Handle;
                }

                _textureBindings[stageIndex, binding + i] = new MetalTextureBinding
                {
                    TextureHandle = textureHandle,
                    SamplerHandle = samplerHandle,
                };
            }
        }

        private void ClearTextureArrayBinding(int stageIndex, int binding)
        {
            if ((uint)stageIndex >= MaxShaderStages || (uint)binding >= MaxTextureBindings)
            {
                return;
            }

            int length = _textureArrayLengths[stageIndex, binding];
            if (length <= 0)
            {
                return;
            }

            int clearCount = Math.Min(length, MaxTextureBindings - binding);
            for (int i = 0; i < clearCount; i++)
            {
                _textureBindings[stageIndex, binding + i] = default;
            }

            _textureArrayLengths[stageIndex, binding] = 0;
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
            private MetalPixelFormat _colorFormat;
            private MetalPixelFormat _depthStencilFormat;

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
                _colorFormat = MetalPixelFormat.BGRA8Unorm;
                _depthStencilFormat = MetalPixelFormat.Invalid;
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
            /// 是否存在未处理的清除操作。
            /// </summary>
            public bool HasPending()
            {
                for (int i = 0; i < MaxColorAttachments; i++)
                {
                    if (_pendingColorClears[i].Active)
                    {
                        return true;
                    }
                }

                return _pendingDepthClear.Active;
            }

            /// <summary>
            /// 从 GAL 接口更新渲染目标状态。
            /// 提取 ITexture 中的原生 Metal 纹理句柄以供后续 P/Invoke 使用。
            /// </summary>
            public bool Set(
                Span<ITexture> colors,
                ITexture depthStencil,
                out MetalPixelFormat colorFormat,
                out MetalPixelFormat depthStencilFormat)
            {
                int count = Math.Min(colors.Length, MaxColorAttachments);
                int validCount = 0;
                MetalPixelFormat newColorFormat = MetalPixelFormat.BGRA8Unorm;
                MetalPixelFormat newDepthStencilFormat = MetalPixelFormat.Invalid;

                for (int i = 0; i < count; i++)
                {
                    ITexture texture = colors[i];
                    if (texture != null && MetalTexture.TryGetNativeHandle(texture, out nint handle))
                    {
                        _colorHandles[i] = handle;
                        validCount++;

                        if (newColorFormat == MetalPixelFormat.BGRA8Unorm &&
                            MetalTexture.TryGetMetalFormat(texture, out MetalPixelFormat format))
                        {
                            newColorFormat = format;
                        }
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

                    if (MetalTexture.TryGetMetalFormat(depthStencil, out MetalPixelFormat format))
                    {
                        newDepthStencilFormat = format;
                    }
                }
                else
                {
                    _depthStencilHandle = nint.Zero;
                }

                bool formatsChanged = _colorFormat != newColorFormat || _depthStencilFormat != newDepthStencilFormat;
                _colorFormat = newColorFormat;
                _depthStencilFormat = newDepthStencilFormat;
                colorFormat = _colorFormat;
                depthStencilFormat = _depthStencilFormat;
                return formatsChanged;
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
