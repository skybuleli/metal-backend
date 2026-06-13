using Ryujinx.Common.Configuration;
using Ryujinx.Graphics.GAL;
using System;

namespace Ryujinx.Graphics.Metal
{
    internal sealed class MetalWindow : IWindow, IDisposable
    {
        public void ChangeVSyncMode(VSyncMode vSyncMode)
        {
        }

        public void Dispose()
        {
        }

        public void Present(ITexture texture, ImageCrop crop, Action swapBuffersCallback)
        {
            swapBuffersCallback?.Invoke();
        }

        public void SetAntiAliasing(AntiAliasing antialiasing)
        {
        }

        public void SetColorSpacePassthrough(bool colorSpacePassThroughEnabled)
        {
        }

        public void SetScalingFilter(ScalingFilter type)
        {
        }

        public void SetScalingFilterLevel(float level)
        {
        }

        public void SetSize(int width, int height)
        {
        }
    }
}
