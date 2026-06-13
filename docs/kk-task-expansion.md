# kk 报告对后续任务的影响与扩充建议

> 依据文档：`/Users/liliang/Downloads/deliverables_kk-extraction-report.md`
> 目的：把 kk Mesa fork 中可复用的 Metal 后端经验，转成当前仓库可执行的任务拆分和验证顺序。

## 一、结论摘要

- **不改变主线阶段顺序**：下一任务仍然是 `P3.1`，先进入 Ryubing fork 与 GAL 集成。
- **需要扩充子任务**：重点扩充 `P3.1` 之后的接口收口、`P4.1/P4.2` 的前置设计与验证、`P5/P6` 的映射骨架和回归样本。
- **最有价值的资产不是代码搬运**：而是模块拆分方式、编译器单例模式、workaround 系统、Metal 限制清单和状态映射表结构。

## 二、这份报告对我们的直接帮助

### 1. libmetal_bridge 的模块边界更清晰

kk 的 `bridge/` 把 Metal 运行时拆成 `device / compiler / buffer / texture / sampler / queue / command_buffer / encoder / heap / render_state / sync` 等模块。

这对本项目的帮助：

- 可以把 `blueprint/architecture.md` 中较粗的 `MetalDevice / MetalQueue / MetalBuffer / MetalTexture / ShaderCompiler / Presenter` 继续细化。
- 可以提前定义目录和 C ABI 边界，避免在 `MetalNative.cs` 和 `libmetal_bridge` 之间来回返工。

### 2. MTL4Compiler 单例模式可直接降低稳定性风险

报告指出编译器应采用“全局单例 + 引用计数”模式，并给出按 `device → compiler` 复用的思路。

这对本项目的帮助：

- `P3.8` 和 `P4.2.x` 可以在第一次实现时就把生命周期设计正确。
- 避免后续出现多编译器并发导致的随机崩溃，再被迫重构。

### 3. workaround 系统值得尽早制度化

报告中列出的 workaround 集中在：

- compiler singleton
- language version 3.2
- discard guard
- helper invocation lowering
- sample mask

这对本项目的帮助：

- 不把 Metal 特例分散到各文件里。
- 后续验证、回归和用户报错时能快速定位“是功能没做”还是“workaround 开关问题”。

### 4. Metal 限制清单可以提前转成验证任务

报告把纹理限制和 subgroup 限制列成了 checklist，例如：

- 1D 纹理
- texelFetch offset
- cube map LOD
- LOD bias
- helper invocation
- discard
- subgroup ballot / vote / reduce / shuffle

这对本项目的帮助：

- 可以在 `P4.2` 前先做小规模实验，不必等到 `P5/P8` 才在真实游戏里被动发现。
- 这些点非常适合转成 `P6.2` 的回归样本。

### 5. 常量与状态映射表能减少后续接口抖动

报告中整理了：

- 资源对齐约束
- viewport / render target / sample count 上限
- storage mode / compare / blend / cull 等枚举

这对本项目的帮助：

- `P4.1` 的设备与资源层可以更早稳定。
- `P5` 的状态映射表可以先有骨架，再逐步填充 Maxwell/GAL 细节。

## 三、为什么要扩充任务

当前 `PROGRESS.md` 中的 `P3.4/P3.7/P3.8/P4.1/P4.2/P5.1/P6.2` 都是对的，但粒度偏粗，存在两个风险：

- **先写接口，后补设计**：`MetalNative.cs`、`MetalShaderCompiler.cs`、资源层可能先按直觉成型，后面再被编译器单例、opaque handle、枚举映射推翻。
- **先接模拟器，后补 Metal 限制验证**：这样问题会更晚暴露，也更难定位。

因此需要在主线不变的前提下，加几个“设计收口”和“验证前置”任务。

## 四、建议扩充到哪些任务

### Phase 3

- `P3.1a`：收口 `libmetal_bridge` 模块骨架 + C ABI/opaque handle 方案
- `P3.1b`：收口 `MetalShaderCompiler` 单例 + workaround 位掩码设计

作用：

- 为 `P3.4 MetalNative.cs`
- `P3.7 MetalDevice.cs`
- `P3.8 MetalShaderCompiler.cs`

提供稳定的接口基础。

### Phase 4

- `P4.1.0`：固化 Metal 硬件限制常量与资源对齐策略
- `P4.2.0`：建立 MSC/Metal 限制验证矩阵

作用：

- 让 `P4.1.x` 和 `P4.2.x` 在落代码前先把常量、限制和验证面收口。

### Phase 5

- `P5.0`：搭建 Maxwell/GAL→Metal 状态映射表骨架

作用：

- 让 `P5.1-P5.10` 先共享同一套映射表框架，而不是一边实现一边发明枚举关系。

### Phase 6

- `P6.2.0`：将 kk Metal 限制清单转为着色器编译回归样本

作用：

- 把“已知风险”变成自动回归，而不是靠记忆。

## 五、哪些任务必须现在开始做

### 必须现在开始

1. `P3.1`
   原因：当前阶段已经切到 Phase 3，主线不变，先完成 Ryubing fork 和工作分支准备。

2. `P3.1a`
   原因：它直接决定 `MetalNative.cs` 和 `libmetal_bridge` 的接口形状，越早收口越少返工。

3. `P3.1b`
   原因：`MetalShaderCompiler` 的单例与 workaround 设计不宜后补，否则 `P3.8/P4.2` 很容易返工。

### 不必马上开始，但应前移

- `P4.1.0`
- `P4.2.0`
- `P5.0`
- `P6.2.0`

这些任务不需要在今天立刻做完，但都应该保留在原阶段的最前面，作为后续实现的前置约束。

## 六、执行顺序建议

建议顺序如下：

1. `P3.1`：Fork Ryubing + feature/native-metal-backend 分支
2. `P3.1a`：收口 bridge 骨架与 C ABI
3. `P3.1b`：收口 compiler 单例与 workaround 设计
4. `P3.2-P3.10`：开始真正的 C# / P/Invoke / stub 落地
5. `P4.1.0`：先固化硬件常量与资源对齐
6. `P4.2.0`：先验证 MSC/Metal 限制矩阵
7. `P4.2.1+`：再做编译器与回退逻辑实现

## 七、落地结果

本次已将上述建议同步进 `PROGRESS.md`，新增任务如下：

- `P3.0`
- `P3.1a`
- `P3.1b`
- `P4.1.0`
- `P4.2.0`
- `P5.0`
- `P6.2.0`

其中：

- `P3.0` 用于记录这次基于 kk 报告的任务收口工作
- 后续实际执行从 `P3.1` 开始
