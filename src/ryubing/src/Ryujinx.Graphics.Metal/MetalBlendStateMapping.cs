using Ryujinx.Graphics.GAL;

namespace Ryujinx.Graphics.Metal
{
    /// <summary>
    /// GAL 混合状态到 Metal 混合附件描述符的集中映射。
    ///
    /// 这层负责把 Ryubing 的 <see cref="BlendDescriptor"/> /
    /// <see cref="AdvancedBlendDescriptor"/> 收敛成 Metal
    /// <see cref="MetalBlendAttachmentDescriptor"/>，供 <see cref="MetalPipeline"/>
    /// 直接缓存并重建 pipeline。
    /// </summary>
    internal static class MetalBlendStateMapping
    {
        public static MetalBlendAttachmentDescriptor CreateDisabledAttachment()
        {
            return new MetalBlendAttachmentDescriptor
            {
                BlendingEnabled = 0,
                ReservedPad0 = 0,
                ReservedPad1 = 0,
                ReservedPad2 = 0,
                SrcRgbFactor = MetalBlendFactor.One,
                DstRgbFactor = MetalBlendFactor.Zero,
                RgbOperation = MetalBlendOperation.Add,
                SrcAlphaFactor = MetalBlendFactor.One,
                DstAlphaFactor = MetalBlendFactor.Zero,
                AlphaOperation = MetalBlendOperation.Add,
                WriteMask = (uint)MetalColorWriteMask.All,
            };
        }

        public static MetalBlendAttachmentDescriptor CreateFallbackAdvancedAttachment(bool enabled)
        {
            MetalBlendAttachmentDescriptor descriptor = CreateDisabledAttachment();
            descriptor.BlendingEnabled = (byte)(enabled ? 1 : 0);
            if (enabled)
            {
                descriptor.SrcRgbFactor = MetalBlendFactor.SrcAlpha;
                descriptor.DstRgbFactor = MetalBlendFactor.OneMinusSrcAlpha;
                descriptor.SrcAlphaFactor = MetalBlendFactor.One;
                descriptor.DstAlphaFactor = MetalBlendFactor.OneMinusSrcAlpha;
            }

            return descriptor;
        }

        public static MetalBlendAttachmentDescriptor CreateAttachment(BlendDescriptor blend)
        {
            MetalBlendAttachmentDescriptor descriptor = CreateDisabledAttachment();

            if (!blend.Enable)
            {
                return descriptor;
            }

            descriptor.BlendingEnabled = 1;
            descriptor.SrcRgbFactor = ToMetalBlendFactor(blend.ColorSrcFactor);
            descriptor.DstRgbFactor = ToMetalBlendFactor(blend.ColorDstFactor);
            descriptor.RgbOperation = ToMetalBlendOp(blend.ColorOp);
            descriptor.SrcAlphaFactor = ToMetalBlendFactor(blend.AlphaSrcFactor);
            descriptor.DstAlphaFactor = ToMetalBlendFactor(blend.AlphaDstFactor);
            descriptor.AlphaOperation = ToMetalBlendOp(blend.AlphaOp);
            return descriptor;
        }

        private static MetalBlendFactor ToMetalBlendFactor(BlendFactor factor)
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

        private static MetalBlendOperation ToMetalBlendOp(BlendOp op)
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
    }
}
