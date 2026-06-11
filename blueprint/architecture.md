# 架构规格

> 从完整蓝图精简而来。完整文档：`deliverables/switch-macos-metal-backend-blueprint.html`

## 着色器管线：五条路径

```
Maxwell SASS → Ryujinx 解码器 → 结构化 IR
    │
    ├── CommandMapper → Slang 原生语法 (HLSL 风格)
    │       ├── 路径 A（主⭐）：→ Slang -target dxil → DXIL → MSC → metallib
    │       ├── 路径 B（备选）：→ Slang -target metal → MSL → xcrun metal → metallib
    │       └── 路径 D（优化）：→ Slang -target spirv → SPIR-V → spirv-opt → Slang -target dxil → MSC
    │
    ├── CodeGen/Glsl → GLSL
    │       └── 路径 C（SPIR-V桥）：→ glslangValidator → SPIR-V → spirv-opt → SPIRV-Cross → MSL
    │
    └── 路径 E（交叉验证）：→ shader-compiler-rs → GLSL' → 任意路径
```

> **P1 实验结论**: CommandMapper 输出 Slang 原生语法而非 GLSL，因为 slangc DXIL 对 GLSL 的 std140/push_constant 不兼容，而 Slang 原生语法与 DXIL SM 6.0 完全对齐。详见 docs/shader-debug.md。

回退策略：路径 A → 路径 C → 路径 B

## Maxwell 硬件参考：三层体系

- **第一层（寄存器级）**：deko3d engine_3d.def + envytools rnndb
- **第二层（指令级）**：NVK Maxwell 后端 + MaxAs + nvdisasm
- **第三层（着色器级）**：Ryujinx 解码器 + shader-compiler-rs

## 运行时架构

```
libmetal_bridge/ (C++)
├── MetalDevice     — MTLDevice 创建、GPU 选择
├── MetalQueue      — MTLCommandQueue 管理
├── MetalBuffer     — MTLBuffer（Managed/Private/Shared 模式）
├── MetalTexture    — MTLTexture + 像素格式映射表
├── ShaderCompiler  — Slang + libmetalirconverter + 缓存
├── CommandMapper   — Maxwell 状态 → Metal API
└── Presenter       — CAMetalLayer、交换链
```

## Ryubing 集成方式

在 C# 中实现 `Ryujinx.Graphics.GAL` 命名空间下的 `IRenderer` + `IPipeline` 接口。C# 层通过 P/Invoke（`MetalNative.cs`）调用 C++ 的 libmetal_bridge。

## 关键架构决策（ADR）

- **ADR-001**：Slang+MSC 为主着色器路径（不用 airconv — 不存在 Maxwell→DXBC 编译器）
- **ADR-002**：Ryubing 为模拟器基础（不用 Astris — 无源码）
- **ADR-003**：C++/C# 混合架构
- **ADR-004**：五条路径冗余，自动回退

## 环境约束（Phase 0 已验证）

- **开发设备**: M1 Mac，8GB RAM / 256GB SSD，仅安装 Xcode Command Line Tools（无 Xcode.app）
- **路径可用性**:
  - ✅ 路径 A：Slang→DXIL→MSC→metallib — 完全可用，已验证端到端
  - ✅ 路径 D：Slang→SPIR-V→opt→DXIL→MSC — 完全可用
  - ❌ 路径 B/C：需 `xcrun metal`（仅含于完整 Xcode），暂不可用，非阻塞
- **MSC 状态**: ✅ 已确认在仅有 CLT SDK 环境下完全可用（P0.1a 验证通过），Apple 文档说需 Xcode 15+ 但实际不依赖
- **metal-cpp**: MTLDevice.h 在 CLT SDK 中存在（P0.1b 验证通过），可直接编译
- **调试限制**: 无 Instruments / GPU Frame Capture（需完整 Xcode），可使用 RenderDoc Metal 插件替代

## 参考实现

- **Ryubing** (`~/dev/ryubing/`): Nintendo Switch 模拟器，C#，GAL 抽象层（IRenderer/IPipeline）
- **dxmt** (`~/dev/dxmt/`): 3Shain/dxmt v0.80，C++，DirectX→Metal 翻译层，Device/Queue/Buffer/Presenter 参考
