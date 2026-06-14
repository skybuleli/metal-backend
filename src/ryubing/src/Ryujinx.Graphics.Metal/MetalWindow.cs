using Ryujinx.Common.Configuration;
using Ryujinx.Graphics.GAL;
using System;
using System.IO;
using System.Runtime.InteropServices;
using Ryujinx.Common.Logging;


namespace Ryujinx.Graphics.Metal
{
    internal sealed class MetalWindow : IWindow, IDisposable
    {
        internal bool ScreenCaptureRequested { get; set; }
        internal Action<ScreenCaptureImageInfo> ScreenCapturedCallback { get; set; }

        private readonly nint _deviceHandle;
        private MetalPipeline _pipeline;
        private nint _metalLayer;
        private bool _firstPresent = true;
        private bool _firstTextureDiagnostic = true;
        private nint _presenterHandle;
        private uint _presenterWidth;
        private uint _presenterHeight;

        internal MetalWindow(nint deviceHandle)
        {
            _deviceHandle = deviceHandle;
        }

        internal void SetPipeline(MetalPipeline pipeline)
        {
            _pipeline = pipeline;
        }

        /// <summary>
        /// 设置 CAMetalLayer 指针（由 AppHost/窗口系统层传入）。
        /// </summary>
        internal void SetLayer(nint metalLayer)
        {
            _metalLayer = metalLayer;
        }

        public void ChangeVSyncMode(VSyncMode vSyncMode)
        {
        }

        public void Dispose()
        {
            if (_presenterHandle != nint.Zero)
            {
                MetalNative.Release(_presenterHandle);
                _presenterHandle = nint.Zero;
            }
            // 确保所有 CommandBuffer 在销毁前提交
            _pipeline?.Flush();

        }

        public void Present(ITexture texture, ImageCrop crop, Action swapBuffersCallback)
        {
            // 仅首次 Present 时记录
            // 提交当前帧的 CommandBuffer（确保所有 Draw 完成后再 Present）
            _pipeline?.Flush();
            if (_firstPresent)
            {
                _firstPresent = false;
                Logger.Info?.PrintMsg(LogClass.Gpu, $"[DIAG] 首次 Present, layer={_metalLayer:X}, presenter={_presenterHandle:X}");
            }

            // 延迟创建 Presenter：需要 CAMetalLayer 就绪
            if (_presenterHandle == nint.Zero && _deviceHandle != nint.Zero && _metalLayer != nint.Zero)
            {
                MetalResult result = MetalNative.CreatePresenter(_deviceHandle, _metalLayer, out nint presenter);
                if (result == MetalResult.Ok)
                {
                    _presenterHandle = presenter;
                    Logger.Info?.PrintMsg(LogClass.Gpu, $"[DIAG] CreatePresenter 成功, presenter={presenter:X}, layer={_metalLayer:X}");
                    if (_presenterWidth > 0 && _presenterHeight > 0)
                    {
                        MetalResult resizeResult = MetalNative.PresenterResize(presenter, _presenterWidth, _presenterHeight);
                        Logger.Info?.PrintMsg(LogClass.Gpu, $"[DIAG] PresenterResize({_presenterWidth}x{_presenterHeight}) -> {resizeResult}");
                    }
                }
                else
                {
                    Logger.Error?.PrintMsg(LogClass.Gpu, $"[DIAG] CreatePresenter 失败: {result}, layer={_metalLayer:X}");
                }
            }

            // 截屏：在 Present 前读取帧缓冲
            if (ScreenCaptureRequested && texture is MetalTexture captureTexture && ScreenCapturedCallback != null)
            {
                CaptureFrame(captureTexture, crop);
                ScreenCaptureRequested = false;
            }

            // Present 到 Metal 屏幕
            if (_presenterHandle != nint.Zero && texture is MetalTexture presentTexture)
            {
                LogTextureDiagnosticIfNeeded(presentTexture);

                if (_presenterWidth == 0 || _presenterHeight == 0)
                {
                    _presenterWidth = (uint)presentTexture.Width;
                    _presenterHeight = (uint)presentTexture.Height;
                    MetalResult resizeResult = MetalNative.PresenterResize(_presenterHandle, _presenterWidth, _presenterHeight);
                    Logger.Info?.PrintMsg(LogClass.Gpu, $"[DIAG] PresenterResize({_presenterWidth}x{_presenterHeight}) -> {resizeResult}");
                }
                MetalResult presentResult = MetalNative.PresenterPresentTexture(_presenterHandle, presentTexture.Handle);
                if (presentResult != MetalResult.Ok)
                {
                    Logger.Error?.PrintMsg(
                        LogClass.Gpu,
                        $"[DIAG] PresenterPresentTexture 失败: {presentResult}, size={presentTexture.Width}x{presentTexture.Height}, format={presentTexture.Info.Format}");
                }
            }

            swapBuffersCallback?.Invoke();
        }

        private void LogTextureDiagnosticIfNeeded(MetalTexture texture)
        {
            if (!_firstTextureDiagnostic)
            {
                return;
            }

            _firstTextureDiagnostic = false;

            bool hasNonZero = false;
            bool hasVisibleColor = false;
            bool hasOpaquePixel = false;
            int inspectedBytes = 0;
            PinnedSpan<byte> rawData = texture.GetData(0, 0);

            try
            {
                ReadOnlySpan<byte> data = rawData.Get();
                inspectedBytes = Math.Min(data.Length, 4096);
                bool isBgra = texture.Info.Format.IsBgr;
                int bytesPerPixel = texture.Info.BytesPerPixel;

                for (int i = 0; i < inspectedBytes; i++)
                {
                    if (data[i] != 0)
                    {
                        hasNonZero = true;
                    }
                }

                if (bytesPerPixel >= 4)
                {
                    for (int i = 0; i + 3 < inspectedBytes; i += bytesPerPixel)
                    {
                        byte r = data[i + (isBgra ? 2 : 0)];
                        byte g = data[i + 1];
                        byte b = data[i + (isBgra ? 0 : 2)];
                        byte a = data[i + 3];

                        if ((r | g | b) != 0)
                        {
                            hasVisibleColor = true;
                        }

                        if (a != 0)
                        {
                            hasOpaquePixel = true;
                        }

                        if (hasVisibleColor && hasOpaquePixel)
                        {
                            break;
                        }
                    }
                }

                DumpTextureToPpm(texture, data);
            }
            finally
            {
                rawData.Dispose();
            }

            Logger.Info?.PrintMsg(
                LogClass.Gpu,
                $"[DIAG] PresentTexture 首帧: size={texture.Width}x{texture.Height}, format={texture.Info.Format}, bytesPerPixel={texture.Info.BytesPerPixel}, inspectBytes={inspectedBytes}, hasNonZero={hasNonZero}, hasVisibleColor={hasVisibleColor}, hasOpaquePixel={hasOpaquePixel}");
        }

        private void DumpTextureToPpm(MetalTexture texture, ReadOnlySpan<byte> data)
        {
            try
            {
                int width = texture.Width;
                int height = texture.Height;
                int bytesPerPixel = texture.Info.BytesPerPixel;

                if (width <= 0 || height <= 0 || bytesPerPixel < 4)
                {
                    Logger.Warning?.PrintMsg(LogClass.Gpu, "[DIAG] 跳过首帧 PNG 导出：尺寸或像素格式不支持。");
                    return;
                }

                int stride = texture.Info.GetMipStride(0);
                int rowBytes = width * bytesPerPixel;
                byte[] tightData = new byte[height * rowBytes];

                for (int y = 0; y < height; y++)
                {
                    data.Slice(y * stride, rowBytes).CopyTo(tightData.AsSpan(y * rowBytes, rowBytes));
                }

                string path = Path.Combine(Path.GetTempPath(), "ryujinx_metal_first_present.ppm");
                using FileStream stream = File.Open(path, FileMode.Create, FileAccess.Write, FileShare.Read);
                using StreamWriter writer = new(stream, leaveOpen: true);
                writer.WriteLine("P6");
                writer.WriteLine($"{width} {height}");
                writer.WriteLine("255");
                writer.Flush();

                bool isBgra = texture.Info.Format.IsBgr;
                byte[] rgbData = new byte[width * height * 3];

                for (int src = 0, dst = 0; src + 3 < tightData.Length; src += bytesPerPixel, dst += 3)
                {
                    rgbData[dst + 0] = tightData[src + (isBgra ? 2 : 0)];
                    rgbData[dst + 1] = tightData[src + 1];
                    rgbData[dst + 2] = tightData[src + (isBgra ? 0 : 2)];
                }

                stream.Write(rgbData, 0, rgbData.Length);

                Logger.Info?.PrintMsg(LogClass.Gpu, $"[DIAG] 已导出首帧源纹理 PPM: {path}");
            }
            catch (Exception ex)
            {
                Logger.Warning?.PrintMsg(LogClass.Gpu, $"[DIAG] 导出首帧源纹理 PPM 失败: {ex.GetType().Name}: {ex.Message}");
            }
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
            _presenterWidth = (uint)width;
            _presenterHeight = (uint)height;
            if (_presenterHandle != nint.Zero)
            {
                MetalNative.PresenterResize(_presenterHandle, _presenterWidth, _presenterHeight);
            }
        }
    }
}
