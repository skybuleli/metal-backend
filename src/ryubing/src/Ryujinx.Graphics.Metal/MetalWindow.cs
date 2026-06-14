using Ryujinx.Common.Configuration;
using Ryujinx.Graphics.GAL;
using System;
using System.Runtime.InteropServices;

namespace Ryujinx.Graphics.Metal
{
    internal sealed class MetalWindow : IWindow, IDisposable
    {
        internal bool ScreenCaptureRequested { get; set; }
        internal Action<ScreenCaptureImageInfo> ScreenCapturedCallback { get; set; }

        public void ChangeVSyncMode(VSyncMode vSyncMode)
        {
        }

        public void Dispose()
        {
        }

        public void Present(ITexture texture, ImageCrop crop, Action swapBuffersCallback)
        {
            if (ScreenCaptureRequested && texture is MetalTexture metalTexture && ScreenCapturedCallback != null)
            {
                CaptureFrame(metalTexture, crop);
                ScreenCaptureRequested = false;
            }

            swapBuffersCallback?.Invoke();
        }

        private void CaptureFrame(MetalTexture texture, ImageCrop crop)
        {
            int srcX0 = crop.Left;
            int srcY0 = crop.Top;
            int captureWidth = crop.Right - srcX0;
            int captureHeight = crop.Bottom - srcY0;

            // 验证裁剪区域，无效时回退到完整纹理
            if (srcX0 < 0 || srcY0 < 0 || captureWidth <= 0 || captureHeight <= 0 ||
                srcX0 + captureWidth > texture.Width || srcY0 + captureHeight > texture.Height)
            {
                srcX0 = 0;
                srcY0 = 0;
                captureWidth = texture.Width;
                captureHeight = texture.Height;
            }

            // 回读纹理 level 0 数据
            PinnedSpan<byte> rawData = texture.GetData(0, 0);
            if (rawData.Get().Length == 0)
            {
                return;
            }

            try
            {
                ReadOnlySpan<byte> dataSpan = rawData.Get();
                int stride = texture.Info.GetMipStride(0);
                bool isBgra = texture.Info.Format.IsBgr;
                bool isFullTexture = srcX0 == 0 && srcY0 == 0 &&
                                     captureWidth == texture.Width && captureHeight == texture.Height;

                byte[] bitmap;
                if (isFullTexture)
                {
                    bitmap = dataSpan.ToArray();
                }
                else
                {
                    // 裁剪到指定区域
                    int bpp = texture.Info.BytesPerPixel;
                    int rowBytes = captureWidth * bpp;
                    bitmap = new byte[captureHeight * rowBytes];

                    for (int y = 0; y < captureHeight; y++)
                    {
                        int srcOffset = (srcY0 + y) * stride + srcX0 * bpp;
                        int dstOffset = y * rowBytes;
                        dataSpan.Slice(srcOffset, rowBytes).CopyTo(bitmap.AsSpan(dstOffset));
                    }
                }

                ScreenCapturedCallback(new ScreenCaptureImageInfo(
                    captureWidth, captureHeight, isBgra, bitmap, crop.FlipX, crop.FlipY));
            }
            finally
            {
                rawData.Dispose();
            }
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
