using System;

namespace Ryujinx.Graphics.Metal
{
    /// <summary>
    /// GAL 命令/状态到 Metal 实现面的骨架映射表。
    ///
    /// 这一层的目标不是立刻把每个分支都实现完，而是先把“谁负责什么”
    /// 固化下来，避免后续在 <see cref="MetalPipeline"/> 里继续散写状态逻辑。
    /// </summary>
    internal static class MetalStateMapping
    {
        private static readonly MetalStateMappingEntry[] s_entries =
        [
            new("SetProgram", "MTLRenderPipelineState / MTLComputePipelineState", MetalStateDomain.Program, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "当前由 MetalPipeline 直接持有程序与管线缓存，包含 vertex/geometry-as-compute 的 compute 变体。"),
            new("SetRenderTargets", "MTLRenderPassDescriptor", MetalStateDomain.RenderTarget, "MetalPipeline / MetalWindow", "P5.0", MetalStateMaturity.Skeleton, "当前由 MetalRenderTargetState 汇总颜色/深度附件。"),
            new("SetViewports", "setViewport", MetalStateDomain.ViewportScissor, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "现有实现已在 MetalPipeline 中保存视口数组。"),
            new("SetScissors", "setScissorRect", MetalStateDomain.ViewportScissor, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "现有实现已裁剪到当前渲染目标尺寸。"),
            new("SetBlendState(int)", "MTLRenderPipelineColorAttachmentDescriptor", MetalStateDomain.Blend, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "当前通过 blend attachment 缓存重建管线。"),
            new("SetBlendState(Advanced)", "MTLRenderPipelineColorAttachmentDescriptor", MetalStateDomain.Blend, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "高级混合先回退为标准混合，后续再做精细化。"),
            new("SetDepthTest", "MTLDepthStencilState", MetalStateDomain.DepthStencil, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "深度/模板状态已集中到 UpdateDepthStencilState。"),
            new("SetStencilTest", "MTLDepthStencilState", MetalStateDomain.DepthStencil, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "与深度测试共享同一份深度模板状态对象。"),
            new("SetFaceCulling", "setCullMode", MetalStateDomain.Rasterizer, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "面剔除已保存为 cullEnabled + cullMode。"),
            new("SetFrontFace", "setFrontFacingWinding", MetalStateDomain.Rasterizer, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "正反面绕线已保存为 MetalWinding。"),
            new("SetPolygonMode", "setTriangleFillMode", MetalStateDomain.Rasterizer, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "Point 模式暂回退到 Lines。"),
            new("SetPrimitiveTopology", "drawPrimitives / drawIndexedPrimitives", MetalStateDomain.Rasterizer, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "当前仅保存 topology，绘制时换算 MetalPrimitiveType。"),
            new("SetPatchParameters", "PatchControlPoints + tessellation compute fallback", MetalStateDomain.Tessellation, "MetalPipeline", "P5.12", MetalStateMaturity.Partial, "patch 顶点数已缓存，Metal 侧依赖上游 compute 化曲面细分路径承接。"),
            new("SetTransformFeedbackBuffers", "MTLBuffer writeback targets / storage buffer emulation", MetalStateDomain.TransformFeedback, "MetalPipeline", "P5.13", MetalStateMaturity.Partial, "TF 缓冲区已进入 Metal 侧状态缓存，写入由上游 compute 化路径完成。"),
            new("SetVertexAttribs", "MTLVertexDescriptor.attribute", MetalStateDomain.VertexInput, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "已有顶点属性布局收集和管线重建。"),
            new("SetVertexBuffers", "MTLVertexDescriptor.layout / setVertexBuffer", MetalStateDomain.VertexInput, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "已有顶点缓冲布局收集和管线重建。"),
            new("SetIndexBuffer", "setIndexBuffer", MetalStateDomain.VertexInput, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "当前只记录 BufferRange + IndexType。"),
            new("DrawIndirect", "drawPrimitives:indirectBuffer:", MetalStateDomain.VertexInput, "MetalPipeline", "P5.14", MetalStateMaturity.Implemented, "Metal 原生支持单次 indirect draw，已接到 render encoder。"),
            new("DrawIndexedIndirect", "drawIndexedPrimitives:indirectBuffer:", MetalStateDomain.VertexInput, "MetalPipeline", "P5.14", MetalStateMaturity.Implemented, "Metal 原生支持索引 indirect draw，已接到 render encoder。"),
            new("DrawIndirectCount", "indirectBuffer + CPU count fallback", MetalStateDomain.VertexInput, "MetalPipeline", "P5.14", MetalStateMaturity.Partial, "count 版本先读取参数缓冲区并在同一 encoder 内循环发射。"),
            new("DrawIndexedIndirectCount", "indirectBuffer + CPU count fallback", MetalStateDomain.VertexInput, "MetalPipeline", "P5.14", MetalStateMaturity.Partial, "count 版本先读取参数缓冲区并在同一 encoder 内循环发射。"),
            new("SetUniformBuffers", "setVertexBuffer / setFragmentBuffer", MetalStateDomain.ResourceBinding, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "统一缓冲区已按 binding 缓存。"),
            new("SetStorageBuffers", "setVertexBuffer / setFragmentBuffer / setBuffer", MetalStateDomain.ResourceBinding, "MetalPipeline", "P5.8", MetalStateMaturity.Implemented, "存储缓冲区已按 binding 缓存，并在 compute 下发时批量绑定。"),
            new("SetTextureAndSampler", "setFragmentTexture + setFragmentSamplerState", MetalStateDomain.ResourceBinding, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "当前只做 fragment 绑定，后续再扩展 stage-aware 路径。"),
            new("SetImage", "setTexture", MetalStateDomain.ResourceBinding, "MetalPipeline", "P5.8", MetalStateMaturity.Implemented, "compute 图像与纹理数组现在会展开到连续 binding 槽位。"),
            new("DispatchCompute", "MTLComputeCommandEncoder", MetalStateDomain.Compute, "MetalPipeline / 后续 ComputeEncoder", "P5.0", MetalStateMaturity.Skeleton, "计算路径暂未接通，但状态入口已经预留。"),
            new("GeometryAsCompute", "VtgAsCompute + compute metallib", MetalStateDomain.Geometry, "Gpu ShaderCache / MetalPipeline", "P5.11", MetalStateMaturity.Partial, "Ryubing 已在 GPU 侧把几何阶段解构成可选 compute 变体；Metal 侧只需消费 host program 的 compute metallib。"),
            new("CopyBuffer", "MTLBlitCommandEncoder.copyFromBuffer", MetalStateDomain.Copy, "MetalPipeline / MetalResources", "P5.9", MetalStateMaturity.Implemented, "buffer→buffer 复制已接到真实 blit 命令编码器。"),
            new("ClearBuffer", "MTLBlitCommandEncoder.fillBuffer", MetalStateDomain.Clear, "MetalPipeline / MetalResources", "P5.0", MetalStateMaturity.Skeleton, "当前以缓冲区清零/填充的状态入口预留。"),
            new("ClearRenderTargetColor", "MTLRenderPassDescriptor.loadAction = Clear", MetalStateDomain.Clear, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "现有清除参数已缓存到渲染目标状态。"),
            new("ClearRenderTargetDepthStencil", "MTLRenderPassDescriptor.loadAction = Clear", MetalStateDomain.Clear, "MetalPipeline", "P5.0", MetalStateMaturity.Skeleton, "深度/模板清除同样缓存到渲染目标状态。"),
            new("TextureBarrier", "MTLFence / encoder split", MetalStateDomain.Sync, "MetalPipeline / MetalSync", "P5.0", MetalStateMaturity.Skeleton, "先保留同步入口，避免真实游戏时状态缺口。"),
            new("TextureBarrierTiled", "MTLFence / encoder split", MetalStateDomain.Sync, "MetalPipeline / MetalSync", "P5.0", MetalStateMaturity.Skeleton, "Tiled 路径与普通纹理屏障共享同一骨架。"),
            new("Barrier", "CommandBuffer boundary", MetalStateDomain.Sync, "MetalPipeline / MetalSync", "P5.0", MetalStateMaturity.Skeleton, "当前作为命令缓冲边界的占位点。"),
            new("CommandBufferBarrier", "CommandBuffer boundary", MetalStateDomain.Sync, "MetalPipeline / MetalSync", "P5.0", MetalStateMaturity.Skeleton, "为后续资源可见性控制预留。"),
            new("BeginTransformFeedback", "Compute emulation", MetalStateDomain.TransformFeedback, "MetalPipeline", "P5.13", MetalStateMaturity.Partial, "Metal 无原生 TF，但现在会记录活动状态与目标 topology，供 compute 化路径写入 MTLBuffer。"),
            new("EndTransformFeedback", "Compute emulation", MetalStateDomain.TransformFeedback, "MetalPipeline", "P5.13", MetalStateMaturity.Partial, "与 BeginTransformFeedback 成对保留，并结束 TF 活动标记。"),
            new("TryHostConditionalRendering", "MTLCounterSampleBuffer / host fallback", MetalStateDomain.ConditionalRendering, "MetalPipeline / MetalSync", "P5.14", MetalStateMaturity.Partial, "Metal 侧仍回退 CPU 条件判断，但已记录明确诊断，避免误以为 host 路径可用。"),
            new("EndHostConditionalRendering", "MTLCounterSampleBuffer / host fallback", MetalStateDomain.ConditionalRendering, "MetalPipeline / MetalSync", "P5.14", MetalStateMaturity.Partial, "与条件渲染入口配对，当前仅用于结束回退语义。"),
        ];

        public static ReadOnlySpan<MetalStateMappingEntry> Entries => s_entries;

        public static bool TryGetByGalMethod(string galMethod, out MetalStateMappingEntry entry)
        {
            for (int i = 0; i < s_entries.Length; i++)
            {
                if (string.Equals(s_entries[i].GalMethod, galMethod, StringComparison.Ordinal))
                {
                    entry = s_entries[i];
                    return true;
                }
            }

            entry = default;
            return false;
        }
    }

    internal enum MetalStateDomain
    {
        Program,
        RenderTarget,
        ViewportScissor,
        Blend,
        DepthStencil,
        Rasterizer,
        Tessellation,
        VertexInput,
        ResourceBinding,
        Compute,
        Geometry,
        Copy,
        Clear,
        Sync,
        TransformFeedback,
        ConditionalRendering,
    }

    internal enum MetalStateMaturity
    {
        Skeleton,
        Partial,
        Implemented,
    }

    internal sealed record class MetalStateMappingEntry(
        string GalMethod,
        string MetalTarget,
        MetalStateDomain Domain,
        string Owner,
        string PhaseTask,
        MetalStateMaturity Maturity,
        string Notes);
}
