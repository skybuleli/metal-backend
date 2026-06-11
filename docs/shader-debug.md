# 着色器调试速查（Phase 1+）

## GLSL 特性兼容性矩阵（P1 验证结论）

> 任何阶段的着色器编译失败，先查此表。
>
> **重要**: Path A 的输入不限于 GLSL。slangc 同时接受 GLSL、HLSL 和 Slang 原生语法。
> **P4 决策**: CommandMapper 应输出 **Slang 原生语法**（HLSL 风格），绕过 GLSL UBO/push_constant 的兼容性问题。

| GLSL 特性 | Path A (slangc→DXIL→MSC) | Path C (glslang→SPIR-V) | 绕过方案 |
|-----------|:---:|:---:|----------|
| 简单 VS（vec/mat 运算） | ✅ | ✅ | — |
| 简单 FS（颜色输出） | ✅ | ✅ | — |
| texture() 采样 (VS) | ✅ | ✅ | — |
| texture() 采样 (FS) | ✅ (Path A) | ✅ | Path B 的 slang -target metal ❌ |
| FS 使用 `ps_6_0` profile | ✅ | — | P1.7 固定片段阶段 profile |
| CS 使用 `cs_6_0` profile | ✅ | — | P1.8 固定计算阶段 profile |
| RWStructuredBuffer / RWByteAddressBuffer | ✅ (Slang 原生) | — | Path A compute 主语法 |
| groupshared + barrier | ✅ (Slang 原生) | — | P1.8 覆盖 |
| atomic add | ✅ (Slang 原生) | — | P1.8 覆盖 |
| RWTexture2D store | ✅ (Slang 原生) | — | P1.8 覆盖 |
| GLSL compute std430 | ❌ E36107 | ✅ | 作为 Path C 对照语料 |
| layout(std140) UBO | ❌ E36107 | ✅ | 改用 Slang `ConstantBuffer<T>` 语法 |
| push_constant | ❌ E36107 | ✅ | 改用 Slang `[[vk::push_constant]]` 或 constant buffer |
| gl_PointSize (VS) | ❌ SV_PointSize 无效 | ✅ | DXIL SM 6.0 VS 无此语义，需用其他机制 |
| #version 450/460 | ✅ | ✅ | — |
| GL_ARB_separate_shader_objects | ✅ | ✅ | — |
| layout(binding=N) uniform | ✅ | ✅ | 仅 `std140` 块语法不兼容 |
| mat4 * mat4 / mat4 * vec4 | ✅ | ✅ | — |
| if/else + ternary | ✅ | ✅ | — |
| for / while 循环 | ✅ | ✅ | — |
| sin/cos/pow/sqrt/abs | ✅ | ✅ | — |
| clamp/mix/min/max | ✅ | ✅ | — |
| 多 varying 输出 | ✅ | ✅ | — |
| 整数位运算 (&/\|/<</>>) | ✅ | ✅ | — |
| 局部数组 + swizzle | ✅ | ✅ | — |
| normalize() | ✅ | ✅ | — |
| spvFMul 等 SPIR-V helper | — | ✅ (MSL 输出) | 仅 SPIR-V roundtrip 产物 |
| mul() 函数 (HLSL) | ✅ (Slang 原生) | ❌ | GLSL 用 `*`，Slang 用 `mul()` |
| SPV→GLSL roundtrip | ✅ | — | spirv-cross --version 460 → Path A |

### 已知失败模式与绕过方案

| 错误 | 阶段 | 根因 | 绕过 | 未来影响 |
|------|------|------|------|----------|
| `E36107: unavailable features` | slangc DXIL | GLSL std140/push_constant 在 DXIL SM 6.0 无对应语义 | CommandMapper 直接输出 Slang 原生语法（ConstantBuffer<T>、[shader("vertex")]）| P4 已决策：不使用 GLSL 作为 Path A 输入 |
| `SV_PointSize is invalid` | slangc DXIL (dxc) | DXIL SM 6.0 VS 无 gl_PointSize 语义 | 移除 gl_PointSize 或用其他方式传递点大小 | Switch 游戏常用点精灵，P4/P8 需处理 |
| `E36107` (fragment texture) | slangc -target metal | Slang metal 目标 FS 不支持 texture() | Path A 不受影响 | 仅 Path B 不可用，非阻塞 |
| slangc 返回 0 但无 DXIL | slangc DXIL (FS) | 部分简单片段样本使用 `sm_6_0` 时未产物 | 片段阶段固定 `-profile ps_6_0` | P1.7 已在脚本中固化 |
| `E36107` (GLSL compute std430) | slangc DXIL (CS) | GLSL storage buffer/std430 与 Path A 主线不对齐 | CommandMapper 输出 Slang 原生 `RWStructuredBuffer` | P1.8 已作为 Path C 对照语料 |
| entry point parameter treated as uniform | slangc DXIL (CS) | deko3d raw 样本存在无 system-value semantic 的参数 | 后续 CommandMapper 需显式标注系统语义或改为常量缓冲 | P1.8 raw sinewave 仍可通过 Path A |
| `unrecognized source file` | glslangValidator | `.glsl` 后缀无法识别着色器阶段 | 用 `.vert.glsl` / `.frag.glsl` 复合后缀 | 所有脚本均应用复合后缀 |
| MSL 有效但无法编译 metallib | xcrun metal | 需完整 Xcode.app，CLT-only 无此工具 | 用 Path A (MSC) 代替 | P4+ 需安装 Xcode 或使用 MSC |
| mktemp Operation not permitted | 沙箱环境 | macOS 沙箱限制 /tmp 写入 | 回退到 `$SCRIPT_DIR/.tmp_test` | 所有脚本均已内置回退逻辑 |

## Path A 输入语言决策（P1 实验结论）

```
❌ 原假设:  Maxwell → Ryujinx 解码 → GLSL → slangc → DXIL → MSC → metallib
✅ 实际方案: Maxwell → Ryujinx 解码 → Slang 原生语法 → slangc → DXIL → MSC → metallib
```

**原因**: slangc 处理 GLSL 的 std140 UBO 和 push_constant 时无法映射到 DXIL SM 6.0 语义，而 Slang 原生语法（`ConstantBuffer<T>`、`[shader("vertex")]`、`SV_Position`）与 DXIL 完全对齐，不存在兼容性问题。

**Slang 原生语法示例**（CommandMapper 参考模板）：
```slang
struct SceneData {
    float4x4 mvp;
    float4 lightDir;
};

ConstantBuffer<SceneData> scene;   // 替代 GLSL layout(std140) uniform

struct VSInput {
    float3 pos : POSITION;
    float3 normal : NORMAL;
};

struct VSOutput {
    float4 sv_pos : SV_Position;   // 替代 gl_Position + gl_PointSize
    float3 worldNormal : NORMAL;
};

[shader("vertex")]                 // 替代 layout(location=N)
VSOutput main(VSInput input) {
    VSOutput output;
    output.sv_pos = mul(scene.mvp, float4(input.pos, 1.0));
    output.worldNormal = input.normal;
    return output;
}
```

**不需要 IR 中间层**：slangc 本身是完整编译器，内置常量折叠/死代码消除/循环展开，加 IR 层只增加转换损耗。Ryujinx 解码器已提供结构化 Maxwell 指令表示，CommandMapper 可直接生成 Slang。

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
- deko3d 简单 FS：2816B DXIL → 4884B metallib（≈1.73×，P1.7）
- deko3d raw CS：4156B DXIL → 5360B metallib（≈1.29×，P1.8）
- compute 最小 RWBuffer：2956B DXIL → 4612B metallib（≈1.56×，P1.8）
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
