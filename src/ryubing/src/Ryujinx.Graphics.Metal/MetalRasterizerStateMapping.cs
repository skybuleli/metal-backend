using Ryujinx.Graphics.GAL;

namespace Ryujinx.Graphics.Metal
{
    /// <summary>
    /// GAL 光栅化状态到 Metal 光栅化相关枚举/策略的集中映射。
    ///
    /// 这层只处理 Metal 项目可以直接引用的 GAL 公共枚举，避免把 GPU 内部状态类型
    /// 泄漏到 Metal 后端。深度偏移、depth clamp 等状态先由 <see cref="MetalPipeline"/>
    /// 自身缓存，后续任务再补齐具体落点。
    /// </summary>
    internal static class MetalRasterizerStateMapping
    {
        public static MetalCullMode ToMetalCullMode(bool enable, Face face)
        {
            if (!enable)
            {
                return MetalCullMode.None;
            }

            return face switch
            {
                Face.Front => MetalCullMode.Front,
                Face.Back => MetalCullMode.Back,
                Face.FrontAndBack => MetalCullMode.None,
                _ => MetalCullMode.None,
            };
        }

        public static MetalWinding ToMetalWinding(FrontFace frontFace)
        {
            return frontFace switch
            {
                FrontFace.Clockwise => MetalWinding.Clockwise,
                FrontFace.CounterClockwise => MetalWinding.CounterClockwise,
                _ => MetalWinding.CounterClockwise,
            };
        }

        public static MetalTriangleFillMode ToMetalFillMode(PolygonMode frontMode)
        {
            return frontMode switch
            {
                PolygonMode.Fill => MetalTriangleFillMode.Fill,
                PolygonMode.Line => MetalTriangleFillMode.Lines,
                PolygonMode.Point => MetalTriangleFillMode.Lines,
                _ => MetalTriangleFillMode.Fill,
            };
        }
    }
}
