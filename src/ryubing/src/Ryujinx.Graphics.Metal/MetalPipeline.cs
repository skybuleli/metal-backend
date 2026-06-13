using Ryujinx.Graphics.GAL;
using Ryujinx.Graphics.Shader;
using System;
using System.Runtime.InteropServices;

namespace Ryujinx.Graphics.Metal
{
    internal sealed class MetalPipeline : IPipeline
    {
        private IProgram _program;
        private nint _pipelineHandle;
        private readonly nint _deviceHandle;

        /// <summary>
        /// 当前活动的渲染管线句柄（由 metal_create_render_pipeline 返回）
        /// </summary>
        internal nint PipelineHandle => _pipelineHandle;

        public MetalPipeline(nint deviceHandle)
        {
            _deviceHandle = deviceHandle;
            _pipelineHandle = nint.Zero;
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
                    Reserved = new uint[4],
                };

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
        }

        public void SetVertexBuffers(ReadOnlySpan<VertexBufferDescriptor> vertexBuffers)
        {
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
    }
}
