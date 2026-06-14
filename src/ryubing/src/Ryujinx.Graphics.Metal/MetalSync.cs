using Ryujinx.Common.Logging;
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Threading;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    internal sealed class MetalSync : IDisposable
    {
        private sealed class SyncHandle
        {
            public ulong Id;
            public nint SharedEvent;
        }

        private const ulong SignalValue = 1;
        private readonly nint _deviceHandle;
        private readonly nint _queueHandle;
        private readonly List<SyncHandle> _handles;
        private ulong _firstHandle;

        public MetalSync(nint deviceHandle, nint queueHandle)
        {
            _deviceHandle = deviceHandle;
            _queueHandle = queueHandle;
            _handles = [];
        }

        public void Create(ulong id, bool strict)
        {
            MetalResult result = MetalNative.CreateSharedEvent(_deviceHandle, out nint sharedEvent);
            if (result != MetalResult.Ok || sharedEvent == nint.Zero)
            {
                ThrowIfFailed(result, nameof(MetalNative.CreateSharedEvent));
                return;
            }

            nint commandBuffer = nint.Zero;

            try
            {
                result = MetalNative.BeginCommandBuffer(_queueHandle, out commandBuffer);
                if (result != MetalResult.Ok || commandBuffer == nint.Zero)
                {
                    ThrowIfFailed(result, nameof(MetalNative.BeginCommandBuffer));
                    MetalNative.Release(sharedEvent);
                    return;
                }

                result = MetalNative.EncodeSignalSharedEvent(commandBuffer, sharedEvent, SignalValue);
                ThrowIfFailed(result, nameof(MetalNative.EncodeSignalSharedEvent));

                result = MetalNative.CommitCommandBuffer(commandBuffer);
                ThrowIfFailed(result, nameof(MetalNative.CommitCommandBuffer));

                if (strict)
                {
                    // 当前 Metal 后端的 draw 路径本身是同步提交+等待，strict 仅额外确保这个空
                    // command buffer 已经完成，保持与 Vulkan strict sync 的“可立即等待”语义一致。
                    result = MetalNative.WaitCommandBuffer(commandBuffer);
                    ThrowIfFailed(result, nameof(MetalNative.WaitCommandBuffer));
                }

                lock (_handles)
                {
                    _handles.Add(new SyncHandle
                    {
                        Id = id,
                        SharedEvent = sharedEvent,
                    });
                }
            }
            finally
            {
                if (commandBuffer != nint.Zero)
                {
                    MetalNative.Release(commandBuffer);
                }
            }
        }

        public ulong GetCurrent()
        {
            lock (_handles)
            {
                ulong lastHandle = _firstHandle;

                foreach (SyncHandle handle in _handles)
                {
                    if (handle.SharedEvent == nint.Zero || handle.Id <= lastHandle)
                    {
                        continue;
                    }

                    if (TryGetSignaledValue(handle.SharedEvent, out ulong value) && value >= SignalValue)
                    {
                        lastHandle = handle.Id;
                    }
                }

                return lastHandle;
            }
        }

        public void Wait(ulong id)
        {
            SyncHandle result = null;

            lock (_handles)
            {
                if ((long)(_firstHandle - id) > 0)
                {
                    return;
                }

                foreach (SyncHandle handle in _handles)
                {
                    if (handle.Id == id)
                    {
                        result = handle;
                        break;
                    }
                }
            }

            if (result == null || result.SharedEvent == nint.Zero)
            {
                return;
            }

            long startTicks = Stopwatch.GetTimestamp();
            long timeoutTicks = Stopwatch.Frequency;

            while (Stopwatch.GetTimestamp() - startTicks < timeoutTicks)
            {
                if (TryGetSignaledValue(result.SharedEvent, out ulong value) && value >= SignalValue)
                {
                    return;
                }

                Thread.Sleep(1);
            }

            Logger.Error?.PrintMsg(LogClass.Gpu, $"Metal SharedEvent {result.Id} failed to signal within 1000ms. Continuing...");
        }

        public void Cleanup()
        {
            while (true)
            {
                SyncHandle first = null;

                lock (_handles)
                {
                    if (_handles.Count > 0)
                    {
                        first = _handles[0];
                    }
                }

                if (first == null || first.SharedEvent == nint.Zero)
                {
                    break;
                }

                if (!TryGetSignaledValue(first.SharedEvent, out ulong value) || value < SignalValue)
                {
                    break;
                }

                lock (_handles)
                {
                    if (_handles.Count == 0 || _handles[0] != first)
                    {
                        continue;
                    }

                    _firstHandle = first.Id + 1;
                    _handles.RemoveAt(0);
                }

                MetalNative.Release(first.SharedEvent);
                first.SharedEvent = nint.Zero;
            }
        }

        public void Dispose()
        {
            lock (_handles)
            {
                foreach (SyncHandle handle in _handles)
                {
                    if (handle.SharedEvent != nint.Zero)
                    {
                        MetalNative.Release(handle.SharedEvent);
                        handle.SharedEvent = nint.Zero;
                    }
                }

                _handles.Clear();
            }
        }

        private static bool TryGetSignaledValue(nint sharedEvent, out ulong value)
        {
            MetalResult result = MetalNative.GetSharedEventSignaledValue(sharedEvent, out value);
            return result == MetalResult.Ok;
        }

        private static void ThrowIfFailed(MetalResult result, string operation)
        {
            if (result != MetalResult.Ok)
            {
                throw new InvalidOperationException($"{operation} 失败：{result}");
            }
        }
    }
}
