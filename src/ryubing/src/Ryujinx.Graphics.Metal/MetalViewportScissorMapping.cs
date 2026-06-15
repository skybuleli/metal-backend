using Ryujinx.Graphics.GAL;
using System;

namespace Ryujinx.Graphics.Metal
{
    /// <summary>
    /// GAL 视口/裁剪状态到 Metal 描述符的集中映射。
    ///
    /// 这层负责把 Ryujinx 的 viewport/scissor 规则统一换算成 Metal 的
    /// <see cref="MetalViewport"/> 与 <see cref="MetalScissorRect"/>，让
    /// <see cref="MetalPipeline"/> 只保存缓存和调用结果。
    /// </summary>
    internal static class MetalViewportScissorMapping
    {
        public static MetalViewport ToMetalViewport(in Viewport viewport)
        {
            float width = viewport.Region.Width == 0f ? 1f : viewport.Region.Width;
            float height = viewport.Region.Height == 0f ? 1f : viewport.Region.Height;

            return new MetalViewport
            {
                OriginX = viewport.Region.X,
                OriginY = Math.Abs(height) - viewport.Region.Y - height,
                Width = width,
                Height = Math.Abs(height),
                ZNear = Math.Clamp(viewport.DepthNear, 0f, 1f),
                ZFar = Math.Clamp(viewport.DepthFar, 0f, 1f),
            };
        }

        public static MetalScissorRect ToMetalScissorRect(in Rectangle<int> region, int renderWidth, int renderHeight)
        {
            uint x = (uint)Math.Max(0, region.X);
            uint y = (uint)Math.Max(0, region.Y);
            uint width = (uint)Math.Max(0, region.Width);
            uint height = (uint)Math.Max(0, region.Height);

            // Metal 要求 scissor 必须落在当前 render pass 范围内。
            // Ryujinx 有时会传入大矩形作为“全屏”标记，因此这里裁到当前目标尺寸。
            if (x + width > (uint)renderWidth)
            {
                width = (uint)Math.Max(0, renderWidth - (int)x);
            }

            if (y + height > (uint)renderHeight)
            {
                height = (uint)Math.Max(0, renderHeight - (int)y);
            }

            return new MetalScissorRect
            {
                X = x,
                Y = y,
                Width = width,
                Height = height,
            };
        }
    }
}
