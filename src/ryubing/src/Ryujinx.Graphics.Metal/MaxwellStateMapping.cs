using System;

namespace Ryujinx.Graphics.Metal
{
    /// <summary>
    /// Maxwell 3D 状态到 Metal 实现面的归档映射表。
    ///
    /// 这份表的作用是把 Ryubing 的真实状态更新点，按 Maxwell 状态域整理成
    /// 一份可追踪的词典，后续 P5.2+ 可以直接沿着这些条目逐项填实。
    ///
    /// 说明：
    /// - 本表只使用仓库内已验证的 Ryubing 代码作为锚点。
    /// - deko3d / envytools 仅作为 Maxwell 状态语义的外部校验源；具体寄存器编号
    ///   需要后续在参考仓库或本地摘录中逐项补实，当前不在此处臆测。
    /// </summary>
    internal static class MaxwellStateMapping
    {
        private static readonly MaxwellStateMappingEntry[] s_entries =
        [
            new(
                "FaceState",
                "YControl + FaceState",
                "UpdateFaceState() / UpdateFrontFace()",
                "setCullMode + setFrontFacingWinding",
                MaxwellStateDomain.Rasterizer,
                "Rasterizer 状态由是否剔除、剔除面与正面绕线共同决定。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:1214-1234",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:563-570"),
            new(
                "ViewVolumeClipControl",
                "ViewVolumeClipControl + DepthMode",
                "UpdateDepthClampState() / UpdateDepthMode()",
                "setDepthBias / setViewport depth range",
                MaxwellStateDomain.Rasterizer,
                "Depth clamp 与深度范围模式分开处理；当前 Metal 侧先通过现有状态缓存承接。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:685-691",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:551-560"),
            new(
                "PolygonMode",
                "PolygonModeFront + PolygonModeBack + PolygonSmoothEnable",
                "UpdatePolygonMode()",
                "setTriangleFillMode",
                MaxwellStateDomain.Rasterizer,
                "Metal 仅有三角形填充/线框切换，点模式需要回退。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:841-847",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:617-623"),
            new(
                "DepthBias",
                "DepthBiasState + DepthBiasFactor + DepthBiasUnits + DepthBiasClamp",
                "UpdateDepthBiasState()",
                "setDepthBias / setDepthSlopeScale / setDepthClamp",
                MaxwellStateDomain.Rasterizer,
                "多边形偏移由 point/line/fill 三种 enable 位共同决定。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:849-867",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:625-634"),
            new(
                "RasterizeEnable",
                "RasterizeEnable",
                "UpdateRasterizerState()",
                "rasterizer discard",
                MaxwellStateDomain.Rasterizer,
                "禁用栅格化时，Metal 侧对应 rasterizer discard。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:444-450",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:699-707"),
            new(
                "ScissorState",
                "ScissorState + ScreenScissorState",
                "UpdateScissors() / SetScissors()",
                "setScissorRect",
                MaxwellStateDomain.ViewportScissor,
                "Maxwell 的大矩形裁剪需要裁到当前渲染目标范围内。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:640-678",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:272-279"),
            new(
                "ViewportTransform",
                "ViewportTransform + ViewportExtents + YControl",
                "UpdateViewportTransform()",
                "setViewport",
                MaxwellStateDomain.ViewportScissor,
                "视口需要处理 Y 翻转、深度范围和缩放。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:760-821",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:104-171"),
            new(
                "RtControl",
                "RtControl + RtColorState[]",
                "RenderTargetManager / SetRenderTargets()",
                "MTLRenderPassDescriptor",
                MaxwellStateDomain.RenderTarget,
                "颜色附件数量与排列由 draw buffer permutation 决定。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:214-233",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Image/TextureManager.cs:494-511"),
            new(
                "RtColorMask",
                "RtColorMaskShared + RtColorMask[]",
                "UpdateRtColorMask()",
                "MTLRenderPipelineColorAttachmentDescriptor.writeMask",
                MaxwellStateDomain.RenderTarget,
                "写掩码在 Metal 侧对应 attachment 的 color write mask。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:830-838",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:503-517"),
            new(
                "BlendState",
                "BlendIndependent + BlendEnable + BlendStateCommon + BlendState[] + BlendConstant",
                "UpdateBlendState()",
                "MTLRenderPipelineColorAttachmentDescriptor.blending",
                MaxwellStateDomain.Blend,
                "独立混合与统一混合都先收敛为 per-attachment 描述。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:870-933",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:676-709"),
            new(
                "BlendUcodeEnable",
                "BlendUcodeEnable + BlendUcodeConstants + BlendUcodeSize",
                "UpdateBlendState()",
                "advanced blend fallback / emulation",
                MaxwellStateDomain.Blend,
                "高级混合当前仅能作为 host capability 条件分支，Metal 侧先回退到标准混合。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:870-885",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:709-723"),
            new(
                "DepthStencil",
                "DepthTestEnable + DepthWriteEnable + DepthTestFunc + StencilTestState + StencilBackTestState + RtDepthStencilEnable",
                "UpdateDepthTestState() / UpdateStencilTestState()",
                "MTLDepthStencilState",
                MaxwellStateDomain.DepthStencil,
                "深度和模板状态共享同一 host 对象，且模板存在双面测试分支。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:873-923",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:431-452"),
            new(
                "PrimitiveRestart",
                "PrimitiveRestartState + PrimitiveRestartDrawArrays + IndexBufferState",
                "UpdatePrimitiveRestartState()",
                "drawIndexedPrimitives primitive restart handling",
                MaxwellStateDomain.VertexInput,
                "非索引 draw 与索引 draw 对 primitive restart 的约束不同。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:1062-1068",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:538-547"),
            new(
                "VertexInput",
                "VertexBufferState[] + VertexAttribState[] + VbElementU8/U16/U32",
                "UpdateVertexBufferState() / UpdateVertexAttribState()",
                "MTLVertexDescriptor + setVertexBuffer",
                MaxwellStateDomain.VertexInput,
                "顶点属性布局、步进模式和绑定表共同决定 Metal vertex descriptor。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:1240-1280",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:686-709"),
            new(
                "Multisample",
                "MultisampleEnable + MultisampleControl + AlphaToCoverageDitherEnable + RtMsaaMode",
                "UpdateMultisampleState()",
                "MTLRenderPipelineDescriptor.sampleCount",
                MaxwellStateDomain.RenderTarget,
                "当前 Ryubing 侧先把 alpha to coverage / alpha to one 条件写回特化状态。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:1414-1421",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:723-736"),
            new(
                "LogicOp",
                "LogicOpState",
                "UpdateLogicOpState()",
                "host fallback / pipeline specialization",
                MaxwellStateDomain.Blend,
                "Metal 没有直接的逻辑操作管线位，当前以状态记录和分支控制为主。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:1038-1045",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:677-685"),
            new(
                "Clear",
                "ClearColors + ClearDepthValue + ClearStencilValue + ClearFlags",
                "ClearRenderTargetColor() / ClearRenderTargetDepthStencil()",
                "MTLRenderPassDescriptor.loadAction = Clear",
                MaxwellStateDomain.RenderTarget,
                "清除参数缓存到渲染目标状态，后续在 begin render encoding 时合并。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:187-213",
                "src/ryubing/src/Ryujinx.Graphics.Metal/MetalPipeline.cs:138-146"),
            new(
                "Texture/Sampler",
                "TexturePoolState + SamplerPoolState + ShaderState + UniformBufferBind*",
                "UpdateShaderState() / SetTextureAndSampler()",
                "setFragmentTexture + setFragmentSamplerState",
                MaxwellStateDomain.ResourceBinding,
                "纹理/采样器绑定属于 shader 资源域，不是独立的渲染状态。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/StateUpdater.cs:1455-1535",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:733-787"),
            new(
                "Compute/Sync",
                "RenderEnableAddress + RenderEnableCondition + Semaphore* + TextureBarrier + TextureBarrierTiled",
                "Barrier() / TextureBarrier() / TextureBarrierTiled()",
                "MTLFence / command buffer boundary",
                MaxwellStateDomain.Sync,
                "这部分更多是命令流同步语义，不是单个管线状态。",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:137-158",
                "src/ryubing/src/Ryujinx.Graphics.Gpu/Engine/Threed/ThreedClassState.cs:597-605"),
        ];

        public static ReadOnlySpan<MaxwellStateMappingEntry> Entries => s_entries;

        public static bool TryGetByMaxwellState(string maxwellState, out MaxwellStateMappingEntry entry)
        {
            for (int i = 0; i < s_entries.Length; i++)
            {
                if (string.Equals(s_entries[i].MaxwellState, maxwellState, StringComparison.Ordinal))
                {
                    entry = s_entries[i];
                    return true;
                }
            }

            entry = default;
            return false;
        }
    }

    internal enum MaxwellStateDomain
    {
        Rasterizer,
        ViewportScissor,
        RenderTarget,
        Blend,
        DepthStencil,
        VertexInput,
        ResourceBinding,
        Sync,
    }

    internal sealed record class MaxwellStateMappingEntry(
        string MaxwellState,
        string MaxwellFields,
        string RyubingUpdateMethod,
        string MetalTarget,
        MaxwellStateDomain Domain,
        string Notes,
        string RyubingAnchor,
        string StateAnchor);
}
