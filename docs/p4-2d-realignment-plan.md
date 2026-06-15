# Phase 4 重排方案：先打通 2D 最小闭环，再回到蔚蓝

## 背景

当前蔚蓝（Celeste）调试长期无法稳定推进，核心原因不是单个 bug 难修，而是同时混入了三类高耦合变量：

1. 着色器输入路线是否正确
2. 渲染管线与状态映射是否完整
3. C# GAL → P/Invoke → C++ Metal 桥接是否有功能缺口

如果继续直接拿蔚蓝做主驱动样本，会把“编译失败”“首帧黑屏”“资源绑定错误”“Present 正常但内容错误”混在一起，导致排查成本不断上升。

## 当前判断

### 1. 着色器主路线需要前置收口

蓝图和已有实验已经给出一致结论：

- `blueprint/architecture.md`：主路径是 `Slang 原生语法 → DXIL → MSC → metallib`
- `docs/shader-debug.md`：P4 已决策，`CommandMapper` 应直接输出 Slang 原生语法，而不是 GLSL

但当前实现中，`src/libmetal_bridge/src/ShaderCompiler.cpp` 仍保留了 GLSL 桥接逻辑，且 `src/ryubing/src/Ryujinx.Graphics.Metal/MetalShaderCompiler.cs` 仍把 `TargetLanguage.Glsl` 当作正常输入语言之一。  
这说明“架构决策已明确，但主线实现尚未完全收口”。

### 2. 现阶段更像“系统集成未收敛”，不是“蔚蓝特例”

就现有证据看，更大的风险不是蔚蓝本身太难，而是我们还没有建立一条足够窄、足够可观测的 2D 调试主线：

- 没有把真实游戏样本按复杂度分层
- 没有把“编译正确”和“绘制正确”分成两个独立出口
- 没有形成统一的首帧抓取、附件导出、状态快照证据链

因此继续拿蔚蓝做第一主用例，收益偏低。

## 重排目标

把当前主线调整为：

1. 先收口 Slang 原生主路径
2. 再建立最小 2D 闭环和诊断基线
3. 再用轻量 2D 样本逐级放大覆盖面
4. 最后把蔚蓝降级为“后验回归样本”重新接回

## 新的开发顺序

### A. 主语法路线先收口

目标：Metal 主线只依赖 `Slang 原生语法 → DXIL → MSC → metallib`。

要求：

- `CommandMapper`/生成器输出统一的 Slang 原生模板
- GLSL 不再作为 Path A 正常输入
- GLSL 桥接仅保留为诊断/对照工具，不再参与主渲染成功路径

### B. 先打通“最小 2D 闭环”

目标：先验证最常见的 2D 渲染能力，而不是直接赌真实商业游戏。

建议最小闭环按以下层级递进：

1. 单纹理 Quad
2. Sprite Atlas
3. Alpha Blend + HUD
4. Camera Scroll / Tile Map
5. Render Target Copy / Present / Resize

每一步都需要：

- 首帧导出
- 编译日志
- 关键资源绑定快照
- 绘制/Present 结果证据

### C. 样本梯度从“手写 → Homebrew → 蔚蓝”

建议样本分三层：

1. 手写 2D 参考样本
2. 轻量 Homebrew smoke 样本
3. 蔚蓝等复杂真实项目

推荐顺序：

1. 手写 Sprite/Tileset Smoke
2. `TetrisNX`
3. `Pong-NX` / `Switch Pong`
4. `OpenSupaplex`
5. `NXEngine-evo`
6. `Celeste`

这样做的目的，是让每个阶段只新增少量变量。

## 对蔚蓝的定位调整

蔚蓝不再作为当前阶段的“首个打通目标”，而改为：

- `Phase 4.6` 之后的高价值回归样本
- 用于验证复杂 2D 场景、滚动、透明混合、UI/HUD 和资源切换
- 只有在“Slang 主路径 + 最小 2D 闭环 + 轻量 Homebrew”都打通后才重新推进

## 预期收益

这样重排后，我们能把问题稳定分流成三类：

1. `Shader`：Slang 生成、语义映射、资源声明、MSC 编译
2. `Pipeline`：Blend/Viewport/Scissor/RenderTarget/Copy/Present
3. `Bridge`：托管状态缓存、P/Invoke 参数布局、C++ 句柄生命周期

只要分流成功，蔚蓝就不再是“无底洞”，而只是一个复杂回归样本。
