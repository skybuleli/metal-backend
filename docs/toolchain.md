# 工具链速查表（Phase 0 验证通过）

> **来源**：Phase 0 中逐一验证的工具路径、版本与核心命令。

## 工具清单

| 工具 | 路径 | 版本 | 用途 | 验证任务 |
|------|------|------|------|----------|
| **devkitPro** | `/opt/devkitpro/` | 最新 | Switch 开发工具链（含 deko3d） | P0.1 |
| **slangc** | `slangc`（PATH） | 最新 | GLSL→DXIL/SPIR-V/Metal 编译器 | P0.2 |
| **metal-shaderconverter** | `/usr/local/bin/metal-shaderconverter` | 4.0 | DXIL→metallib 转换（Path A 核心） | P0.3 |
| **libmetalirconverter** | `/usr/local/lib/libmetalirconverter.dylib` | 4.0 | MSC 运行时库（P/Invoke 调用） | P0.3 |
| **spirv-as** | `/opt/homebrew/bin/spirv-as` | 最新 | SPIR-V 汇编器 | P0.4 |
| **spirv-dis** | `/opt/homebrew/bin/spirv-dis` | 最新 | SPIR-V 反汇编器 | P0.4 |
| **spirv-val** | `/opt/homebrew/bin/spirv-val` | 最新 | SPIR-V 验证器 | P0.4 |
| **spirv-opt** | `/opt/homebrew/bin/spirv-opt` | 最新 | SPIR-V 优化器 | P0.4 |
| **spirv-cross** | `/opt/homebrew/bin/spirv-cross` | 最新 | SPIR-V→MSL 转换 | P0.4 |
| **spirv-link** | `/opt/homebrew/bin/spirv-link` | 最新 | SPIR-V 链接器 | P0.4 |
| **glslangValidator** | `/opt/homebrew/bin/glslangValidator` | 11.16.3.0 | GLSL→SPIR-V 编译器 | P0.5 |
| **rustc** | `rustc`（PATH） | 1.95.0 | Rust 编译器 | P0.6 |
| **cargo** | `cargo`（PATH） | 1.95.0 | Rust 包管理 | P0.6 |
| **dotnet** | `dotnet`（PATH） | 10.0.101 | .NET SDK（Ryubing 构建） | P0.8 |
| **xcodebuild** | `xcodebuild`（PATH） | CLT only | Apple 构建工具（仅 CLT） | P0.1a |

## 着色器路径命令速查

### Path A — 主路径（Slang→DXIL→MSC→metallib）✅

```bash
slangc input.glsl -target dxil -entry main -stage vertex -profile sm_6_0 -o output.dxil
metal-shaderconverter output.dxil -o output.metallib
```

> 注意：必须加 `-profile sm_6_0`，否则 slangc 不生成 DXIL。

### Path C — SPIR-V 桥接 ✅

```bash
glslangValidator -V input.glsl -o output.spv
spirv-val output.spv
spirv-opt -O output.spv -o opt.spv
spirv-cross output.spv --msl --msl-version 30000 --output output.msl
```

### Path B — Slang→MSL→metallib ❌ 暂不可用

需 `xcrun metal`（仅完整 Xcode 包含）。

### Slang 通用技巧

```bash
slangc -targets                                    # 列出所有目标
slangc input.glsl -target dxil -target spirv \     # 多目标输出
       -entry main -stage vertex -profile sm_6_0
```

## 已知陷阱

| 陷阱 | 表现 | 解决方案 |
|------|------|----------|
| slangc DXIL 不生成输出 | 无文件 | 加 `-profile sm_6_0` |
| MSC CLT 环境 | Apple 说需 Xcode 15+ | ✅ CLT SDK 足够 |
| 路径 B/C 不可用 | xcrun 报错 | 仅用 Path A |
| Ryubing 构建目录 | 无 RID 子目录 | 不带 `-r` 正确行为 |
| dxmt 仓库混淆 | 多个同名仓库 | 用 `3Shain/dxmt` |

## 环境约束（已验证）

- MSC 在 CLT-only 下完全可用（DXIL→metallib 2840→5804）
- MTLDevice.h 在 CLT SDK 中存在
- 路径 A 端到端可用：slangc→DXIL→MSC→metallib，6056 字节
- 路径 B/C 需完整 Xcode，非阻塞
- Ryubing：dotnet 10.0.101，`dotnet build -c Release` 22.5s

## 参考仓库

| 仓库 | 本地路径 | 用途 |
|------|----------|------|
| Ryubing | `~/dev/ryubing/` | Switch 模拟器 C# 源码 |
| dxmt | `~/dev/dxmt/` | 3Shain/dxmt v0.80 DirectX→Metal 翻译层 |
