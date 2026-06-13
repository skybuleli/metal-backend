using Ryujinx.Graphics.GAL;
using Ryujinx.Graphics.Shader;
using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using System.Runtime.Versioning;

namespace Ryujinx.Graphics.Metal
{
    [SupportedOSPlatform("macos")]
    internal sealed class MetalShaderCompiler : IDisposable
    {
        private MetalDevice _device;
        private nint _compilerHandle;
        private MetalShaderCompilerConfig _config;
        private MetalWorkaroundFlags _activeWorkarounds;

        public MetalWorkaroundFlags ActiveWorkarounds => _activeWorkarounds;

        public void AttachDevice(MetalDevice device)
        {
            ArgumentNullException.ThrowIfNull(device);

            if (ReferenceEquals(_device, device) && _compilerHandle != nint.Zero)
            {
                return;
            }

            ReleaseCompiler();

            _device = device;

            MetalResult result = MetalNative.AcquireShaderCompiler(device.Handle, out nint compilerHandle);

            if (result != MetalResult.Ok)
            {
                throw CreateException(nameof(MetalNative.AcquireShaderCompiler), result);
            }

            result = MetalNative.GetDefaultShaderCompilerConfig(out MetalShaderCompilerConfig config);

            if (result != MetalResult.Ok)
            {
                MetalNative.Release(compilerHandle);
                throw CreateException(nameof(MetalNative.GetDefaultShaderCompilerConfig), result);
            }

            config.AbiVersion = MetalNative.AbiVersion;

            result = MetalNative.ConfigureShaderCompiler(compilerHandle, config);

            if (result != MetalResult.Ok)
            {
                MetalNative.Release(compilerHandle);
                throw CreateException(nameof(MetalNative.ConfigureShaderCompiler), result);
            }

            _compilerHandle = compilerHandle;
            _config = config;
            _activeWorkarounds = (MetalWorkaroundFlags)MetalNative.ShaderCompilerGetWorkarounds(compilerHandle);
        }

        public IProgram CreateProgram(ShaderSource[] shaders, ShaderInfo info)
        {
            ArgumentNullException.ThrowIfNull(shaders);

            if (shaders.Length == 0)
            {
                return new MetalProgram(Array.Empty<MetalProgram.CompiledShader>(), info, _activeWorkarounds);
            }

            var compiledStages = new List<MetalProgram.CompiledShader>();
            bool hasFragment = false;
            bool compileFailed = false;

            foreach (ShaderSource shader in shaders)
            {
                ShaderStage stage = shader.Stage;
                if (stage == ShaderStage.Fragment)
                {
                    hasFragment = true;
                }

                // 如果已有二进制缓存，跳过编译
                if (shader.BinaryCode != null && shader.BinaryCode.Length > 0)
                {
                    compiledStages.Add(new MetalProgram.CompiledShader
                    {
                        Stage = stage,
                        Metallib = shader.BinaryCode
                    });
                    continue;
                }

                // 没有源码也无法编译
                if (string.IsNullOrEmpty(shader.Code))
                {
                    compiledStages.Add(new MetalProgram.CompiledShader
                    {
                        Stage = stage,
                        Metallib = Array.Empty<byte>()
                    });
                    continue;
                }

                // 从源码编译：Slang 原生语法 → DXIL → MSC → metallib
                string stageStr = stage switch
                {
                    ShaderStage.Vertex   => "vertex",
                    ShaderStage.Fragment => "fragment",
                    ShaderStage.Compute  => "compute",
                    _ => ""
                };

                string profile = stage switch
                {
                    ShaderStage.Vertex   => "sm_6_0",
                    ShaderStage.Fragment => "ps_6_0",
                    ShaderStage.Compute  => "cs_6_0",
                    _ => ""
                };

                if (string.IsNullOrEmpty(stageStr) || string.IsNullOrEmpty(profile))
                {
                    compileFailed = true;
                    continue;
                }

                MetalShaderCompileResult compileResult =
                    MetalNative.CompileShader(_compilerHandle, shader.Code, stageStr, "main", profile);

                if (compileResult.Result != MetalResult.Ok || compileResult.MetallibData == nint.Zero)
                {
                    // 编译失败记录
                    string compileErr = compileResult.Result != MetalResult.Ok
                        ? $" {compileResult.Result}: {compileResult.ErrorMessage}"
                        : " metallib 数据为空";
                    Console.Error.WriteLine(
                        $"[MetalShaderCompiler] {stage}/{profile} 编译失败：{compileErr}");
                    compileFailed = true;
                    continue;
                }

                byte[] metallib = new byte[compileResult.MetallibSize];
                Marshal.Copy(compileResult.MetallibData, metallib, 0, (int)compileResult.MetallibSize);
                MetalNative.FreeShaderData(compileResult.MetallibData);

                compiledStages.Add(new MetalProgram.CompiledShader
                {
                    Stage = stage,
                    Metallib = metallib
                });
            }

            return new MetalProgram(compiledStages.ToArray(), info, _activeWorkarounds)
            {
                HasFragmentShader = hasFragment,
                LinkStatus = compileFailed ? ProgramLinkStatus.Failure : ProgramLinkStatus.Success
            };
        }

        public IProgram LoadProgramBinary(byte[] programBinary, bool hasFragmentShader, ShaderInfo info)
        {
            ArgumentNullException.ThrowIfNull(programBinary);

            return new MetalProgram(programBinary, hasFragmentShader, info, _activeWorkarounds);
        }

        public void Dispose()
        {
            ReleaseCompiler();
        }

        private void ReleaseCompiler()
        {
            if (_compilerHandle != nint.Zero)
            {
                MetalNative.Release(_compilerHandle);
                _compilerHandle = nint.Zero;
            }

            _activeWorkarounds = default;
            _config = default;
            _device = null;
        }

        private static InvalidOperationException CreateException(string operation, MetalResult result)
        {
            nint errorMessagePtr = MetalNative.GetLastErrorMessage();
            string errorMessage = errorMessagePtr != nint.Zero
                ? Marshal.PtrToStringAnsi(errorMessagePtr) ?? "未知错误。"
                : "未提供错误消息。";

            return new InvalidOperationException($"{operation} 失败：{result}，{errorMessage}");
        }
    }

    internal sealed class MetalProgram : IProgram
    {
        private readonly byte[] _binary;
        private readonly CompiledShader[] _compiledShaders;

        public ProgramLinkStatus LinkStatus { get; internal set; }

        public MetalWorkaroundFlags ActiveWorkarounds { get; }

        public int FragmentOutputMap { get; }

        public bool HasFragmentShader { get; internal set; }

        /// <summary>
        /// 从源码编译结果构造（由 CreateProgram 使用）
        /// </summary>
        public MetalProgram(CompiledShader[] compiledShaders, ShaderInfo info, MetalWorkaroundFlags activeWorkarounds)
        {
            _compiledShaders = compiledShaders ?? Array.Empty<CompiledShader>();
            FragmentOutputMap = info.FragmentOutputMap;
            ActiveWorkarounds = activeWorkarounds;
            LinkStatus = ProgramLinkStatus.Success;

            // 合并所有 metallib 到 _binary 供 GetBinary() 序列化
            _binary = MergeMetallibs(_compiledShaders);
        }

        /// <summary>
        /// 从缓存二进制构造（由 LoadProgramBinary 使用）
        /// </summary>
        public MetalProgram(byte[] binary, bool hasFragmentShader, ShaderInfo info, MetalWorkaroundFlags activeWorkarounds)
        {
            _binary = binary;
            _compiledShaders = ParseMetallibs(binary);
            FragmentOutputMap = info.FragmentOutputMap;
            HasFragmentShader = hasFragmentShader;
            ActiveWorkarounds = activeWorkarounds;
            LinkStatus = ProgramLinkStatus.Success;
        }

        public ProgramLinkStatus CheckProgramLink(bool blocking)
        {
            return LinkStatus;
        }

        public byte[] GetBinary()
        {
            return _binary ?? Array.Empty<byte>();
        }

        /// <summary>
        /// 获取指定着色器阶段的 metallib 数据（供 P4.3.1 创建 MTLRenderPipelineState 使用）
        /// </summary>
        internal byte[] GetShaderMetallib(ShaderStage stage)
        {
            foreach (var cs in _compiledShaders)
            {
                if (cs.Stage == stage)
                    return cs.Metallib;
            }
            return Array.Empty<byte>();
        }

        public void Dispose()
        {
        }

        // ── 辅助类型与序列化 ──

        internal struct CompiledShader
        {
            public ShaderStage Stage;
            public byte[] Metallib;
        }

        /// <summary>
        /// 将多阶段 metallib 合并为单个 byte 数组（简单格式：4字节 stage数 + 每段 4字节stage+8字节长度+数据）
        /// </summary>
        private static byte[] MergeMetallibs(CompiledShader[] shaders)
        {
            if (shaders == null || shaders.Length == 0)
                return Array.Empty<byte>();

            int headerSize = 4; // stage count
            int entryHeaderSize = 4 + 8; // stage (int) + size (long)

            long totalSize = headerSize;
            foreach (var s in shaders)
            {
                totalSize += entryHeaderSize + s.Metallib.Length;
            }

            byte[] result = new byte[totalSize];
            int offset = 0;

            // 写入 stage 数量
            WriteInt32(result, ref offset, shaders.Length);

            foreach (var s in shaders)
            {
                WriteInt32(result, ref offset, (int)s.Stage);
                WriteInt64(result, ref offset, s.Metallib.Length);
                Buffer.BlockCopy(s.Metallib, 0, result, offset, s.Metallib.Length);
                offset += s.Metallib.Length;
            }

            return result;
        }

        /// <summary>
        /// 从合并的 byte 数组中还原多阶段 metallib
        /// </summary>
        private static CompiledShader[] ParseMetallibs(byte[] data)
        {
            if (data == null || data.Length < 4)
                return Array.Empty<CompiledShader>();

            int offset = 0;
            int count = BitConverter.ToInt32(data, offset);
            offset += 4;

            if (count <= 0 || count > 64) // 合理性检查
                return Array.Empty<CompiledShader>();

            var result = new CompiledShader[count];
            for (int i = 0; i < count; i++)
            {
                if (offset + 12 > data.Length)
                    break;

                int stage = BitConverter.ToInt32(data, offset);
                offset += 4;
                long size = BitConverter.ToInt64(data, offset);
                offset += 8;

                if (size < 0 || offset + size > data.Length)
                    break;

                byte[] metallib = new byte[size];
                Buffer.BlockCopy(data, offset, metallib, 0, (int)size);
                offset += (int)size;

                result[i] = new CompiledShader
                {
                    Stage = (ShaderStage)stage,
                    Metallib = metallib
                };
            }

            return result;
        }

        private static void WriteInt32(byte[] buffer, ref int offset, int value)
        {
            buffer[offset + 0] = (byte)(value & 0xFF);
            buffer[offset + 1] = (byte)((value >> 8) & 0xFF);
            buffer[offset + 2] = (byte)((value >> 16) & 0xFF);
            buffer[offset + 3] = (byte)((value >> 24) & 0xFF);
            offset += 4;
        }

        private static void WriteInt64(byte[] buffer, ref int offset, long value)
        {
            WriteInt32(buffer, ref offset, (int)(value & 0xFFFFFFFF));
            WriteInt32(buffer, ref offset, (int)((value >> 32) & 0xFFFFFFFF));
        }
    }
}
