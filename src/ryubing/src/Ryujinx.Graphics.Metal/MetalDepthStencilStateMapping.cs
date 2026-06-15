using Ryujinx.Graphics.GAL;

namespace Ryujinx.Graphics.Metal
{
    /// <summary>
    /// GAL 深度/模板状态到 Metal 深度模板描述符的集中映射。
    ///
    /// 这层把 Ryubing 的 <see cref="DepthTestDescriptor"/> /
    /// <see cref="StencilTestDescriptor"/> 收敛成 Metal
    /// <see cref="MetalDepthStencilDescriptor"/>，供 <see cref="MetalPipeline"/>
    /// 在状态变化时直接重建 host depth/stencil state。
    /// </summary>
    internal static class MetalDepthStencilStateMapping
    {
        public static MetalDepthStencilDescriptor CreateDescriptor(
            DepthTestDescriptor depthTest,
            StencilTestDescriptor stencilTest)
        {
            bool depthEnabled = depthTest.TestEnable;
            bool stencilEnabled = stencilTest.TestEnable;

            MetalDepthStencilDescriptor descriptor = new()
            {
                DepthCompareFunction = ToMetalCompareFunction(depthTest.Func),
                DepthWriteEnabled = (byte)(depthTest.WriteEnable ? 1 : 0),
                StencilEnabled = (byte)(stencilEnabled ? 1 : 0),
                ReservedPad0 = 0,
                ReservedPad1 = 0,
                FrontFace = new MetalStencilDescriptor
                {
                    CompareFunction = ToMetalCompareFunction(stencilTest.FrontFunc),
                    StencilFailure = ToMetalStencilOperation(stencilTest.FrontSFail),
                    DepthFailure = ToMetalStencilOperation(stencilTest.FrontDpFail),
                    DepthStencilPass = ToMetalStencilOperation(stencilTest.FrontDpPass),
                    ReadMask = (uint)stencilTest.FrontFuncMask,
                    WriteMask = (uint)stencilTest.FrontMask,
                },
                BackFace = new MetalStencilDescriptor
                {
                    CompareFunction = ToMetalCompareFunction(stencilTest.BackFunc),
                    StencilFailure = ToMetalStencilOperation(stencilTest.BackSFail),
                    DepthFailure = ToMetalStencilOperation(stencilTest.BackDpFail),
                    DepthStencilPass = ToMetalStencilOperation(stencilTest.BackDpPass),
                    ReadMask = (uint)stencilTest.BackFuncMask,
                    WriteMask = (uint)stencilTest.BackMask,
                },
            };

            // 深度测试关闭时，Metal 侧保持 Always + 禁止写入，模板状态继续保留。
            if (!depthEnabled)
            {
                descriptor.DepthCompareFunction = MetalCompareFunction.Always;
                descriptor.DepthWriteEnabled = 0;
            }

            if (!stencilEnabled)
            {
                descriptor.StencilEnabled = 0;
            }

            return descriptor;
        }

        private static MetalCompareFunction ToMetalCompareFunction(CompareOp op)
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

        private static MetalStencilOperation ToMetalStencilOperation(StencilOp op)
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
    }
}
