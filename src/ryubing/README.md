# Ryubing Metal 后端 — C# GAL 实现

> Phase 3 实现。在 Ryubing 的图形抽象层（GAL）上实现 Metal 渲染后端。

## 模块结构（规划）

```
ryubing/
├── Ryujinx.Graphics.Metal/
│   ├── MetalRenderer.cs      # IRenderer 实现（生命周期管理）
│   ├── MetalPipeline.cs      # IPipeline 实现（63 个绘制/状态方法）
│   ├── MetalProgram.cs       # IProgram 实现（着色器程序）
│   ├── MetalTexture.cs       # ITexture 实现
│   ├── MetalSampler.cs       # ISampler 实现
│   ├── MetalBuffer.cs        # BufferHandle 管理
│   ├── MetalNative.cs        # P/Invoke 声明（调用 libmetal_bridge）
│   ├── MetalFormatTable.cs   # Maxwell→MTLPixelFormat 映射表
│   └── MetalEnumMapping.cs   # GAL 枚举→Metal 枚举翻译
└── Ryujinx.Graphics.Metal.Tests/
    └── MetalPipelineTests.cs # 管线 stub 编译验证
```

## 技术栈

- **语言**: C# 12 / .NET 10
- **桥接方式**: P/Invoke（通过 MetalNative.cs 调用 libmetal_bridge 的 C ABI）
- **接口**: 实现 `Ryujinx.Graphics.GAL.IRenderer` 和 `IPipeline`

## 关键设计决策

- **ADR-002**: 以 Ryubing 为模拟器基础（GAL 抽象层完整）
- **ADR-003**: C# 仅负责接口适配，不直接调用 Metal API
- **IPipeline 63 方法**: 全部实现为 stub（Phase 3），逐步实现（Phase 4+）

## 构建

```bash
dotnet build Ryujinx.Graphics.Metal/Ryujinx.Graphics.Metal.csproj -c Release
```

## 参考

- [Ryubing 源码](https://git.ryujinx.app/projects/Ryubing) — GAL 接口定义
- [IPipeline.cs](https://git.ryujinx.app/projects/Ryubing/raw/branch/master/src/Ryujinx.Graphics.GAL/IPipeline.cs)
- `docs/gal-mapping.md` — IPipeline→Metal API 映射速查
