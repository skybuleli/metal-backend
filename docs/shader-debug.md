# 着色器调试速查（Phase 1+）

## GLSL 特性兼容性矩阵（P1 验证结论）

> 任何阶段的着色器编译失败，先查此表。

| GLSL 特性 | Path A (slangc→DXIL→MSC) | Path C (glslang→SPIR-V) | 绕过方案 |
|-----------|:---:|:---:|----------|
| 简单 VS（vec/mat 运算） | ✅ | ✅ | — |
| 简单 FS（颜色输出） | ✅ | ✅ | — |
| texture() 采样 (VS) | ✅ | ✅ | — |
| texture() 采样 (FS) | ✅ (Path A) | ✅ | Path B 的 slang -target metal ❌ |
| layout(std140) UBO | ❌ E36107 | ✅ | 改用 Slang `ConstantBuffer<T>` 语法 |
| push_constant | ❌ E36107 | ✅ | 改用 Slang `[[vk::push_constant]]` 或 constant buffer |
| #version 450/460 | ✅ | ✅ | — |
| GL_ARB_separate_shader_objects | ✅ | ✅ | — |
| layout(binding=N) uniform | ✅ | ✅ | 仅 `std140` 块语法不兼容 |
| mat4 * vec4 矩阵乘法 | ✅ | ✅ | — |
| spvFMul 等 SPIR-V helper | — | ✅ (MSL 输出) | 仅 SPIR-V roundtrip 产物 |
| mul() 函数 (HLSL) | ✅ (Slang 原生) | ❌ | GLSL 用 `*`，Slang 用 `mul()` |

### 已知失败模式与绕过方案

| 错误 | 阶段 | 根因 | 绕过 | 未来影响 |
|------|------|------|------|----------|
| `E36107: unavailable features` | slangc DXIL | GLSL UBO/push_constant 在 DXIL SM 6.0 无对应 | 改用 Slang 原生语法 | P4 CommandMapper 生成着色器时必须用 Slang 语法 |
| `E36107` (fragment texture) | slangc -target metal | Slang metal 目标 FS 不支持 texture() | Path A 不受影响 | 仅 Path B 不可用，非阻塞 |
| `unrecognized source file` | glslangValidator | `.glsl` 后缀无法识别着色器阶段 | 用 `.vert.glsl` / `.frag.glsl` 复合后缀 | 所有脚本均应用复合后缀 |
| MSL 有效但无法编译 metallib | xcrun metal | 需完整 Xcode.app，CLT-only 无此工具 | 用 Path A (MSC) 代替 | P4+ 需安装 Xcode 或使用 MSC |
| mktemp Operation not permitted | 沙箱环境 | macOS 沙箱限制 /tmp 写入 | 回退到 `$SCRIPT_DIR/.tmp_test` | 所有脚本均已内置回退逻辑 |

## SPIR-V 验证（每次编译后强制执行）

```bash
spirv-val output.spv
```

常见失败：
- `"ID xxx does not dominate its use"` → GLSL 变量声明顺序
- `"Capability xxx is not allowed"` → Slang 目标参数不正确
- `"OpEntryPoint interfaces incompletely"` → 着色器接口绑定不完整

## 常见编译错误速查

| 错误 | 原因 | 修复 |
|------|------|------|
| slangc 无输出 | 缺少 `-profile sm_6_0` | 加 `-profile sm_6_0` |
| slangc "unknown target" | 目标名错误 | `slangc -targets` 查看 |
| MSC "unsupported DXIL" | DXIL 版本过高 | 尝试不同 shader model |
| spirv-cross MSL 报错 | 缺 BufferBlock | 加 `--msl-decoration-binding` |
| glslangValidator 语法错 | GLSL 不兼容 | GLSL 460 + Maxwell 兼容子集 |

## 着色器大小健康检查

- metallib ≈ 2× DXIL 大小（正常）
- metallib < DXIL → MSC 转换失败
- 典型：2840B DXIL → 5804B metallib
- deko3d 简单 VS：3176B DXIL → 6216B metallib（≈1.96×）
- Ryujinx 真实 VS（msl_dump）：23~42KB MSL 文本（600~1000 行）

## 调试命令速查

```bash
spirv-dis input.spv -o output.spvasm     # 反汇编
spirv-opt -O input.spv -o output.spv     # 优化
spirv-cross input.spv --msl --msl-version 30000 --output output.msl
```

## 真实着色器数据源

| 数据 | 路径 | 格式 | 用途 |
|------|------|------|------|
| Ryujinx msl_dump | `~/Library/Application Support/Ryujinx/msl_dump/` | MSL 文本 (100 个) | 真实游戏着色器，可直接验证 MSL 语法 |
| Ryujinx metallib_cache | `~/Library/Application Support/Ryujinx/metallib_cache/` | metallib 二进制 (345 个) | 已编译的 Metal 着色器，参考对比 |
| deko3d GLSL 示例 | `~/autommes/deko3d_slang_poc/shaders/` | GLSL 4.50 (7 个) | 可直接跑 Path A 的已知着色器 |

> 详见 `docs/toolchain.md` 测试数据源章节。
