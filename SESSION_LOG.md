# 会话日志索引

> 本文件只保留当前阶段入口、最近滚动记录和归档索引。完整历史按阶段归档到 `docs/session-logs/`。

## 读取规则

- 查当前下一步：先读 `NEXT_TASK.md` 和 `PROGRESS.md`
- 查阶段历史：按阶段读 `docs/session-logs/phase-*.md`
- 查关键技术结论：优先读 `docs/toolchain.md`、`docs/shader-debug.md`、`docs/metal-api.md`、`docs/gal-mapping.md`
- 新会话结束：只在本文件追加最近记录；当记录超过 5 条时，迁移到对应阶段归档

## 阶段归档

| 阶段 | 归档文件 | 内容 |
|------|----------|------|
| Phase 0 | `docs/session-logs/phase-0.md` | 环境与工具链搭建、仓库模板创建 |
| Phase 1 | `docs/session-logs/phase-1.md` | 着色器管道概念验证、状态机循环加固 |
| Phase 2 | `docs/session-logs/phase-2.md` | D1–D8 渐进式渲染 Demo 与证据固化 |
| Phase 3 | `docs/session-logs/phase-3.md` | Ryubing 同步、GAL 骨架、bridge ABI 收口 |
| Phase 4 | `docs/session-logs/phase-4.md` | 核心 Metal 后端的较早实现记录 |

## 当前状态摘要

- 当前阶段：Phase 4 — 核心 Metal 后端实现
- 当前进度：77/144 任务完成
- 下一任务：P4.3.7 — SetRenderTargets: MTLRenderPassDescriptor
- 最近状态机维护：`gen_next_task.py` 会刷新 `PROGRESS.md` 统计区并生成 `NEXT_TASK.md`；`verify_progress.py` 会校验二者一致性

## 最近滚动记录

### 2026-06-13 | P4.3.2 SetVertexBuffers/SetVertexAttribs — 顶点布局映射 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 打通 GAL 顶点属性/顶点缓冲布局到 Metal 渲染管线描述符的映射

### 2026-06-13 | P4.3.3 SetUniformBuffers — MTLBuffer 绑定 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 完成 uniform buffer 绑定状态缓存，打通 `BufferHandle` → 原生 `MTLBuffer` 句柄解析

### 2026-06-13 | P4.3.4 SetTextureAndSampler — 纹理+采样器绑定 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 完成纹理/采样器绑定状态缓存，为后续绘制和 compute encoder 下发绑定做准备

### 2026-06-13 | P4.3.5 SetStorageBuffers — Compute/Graphics 存储缓冲 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 完成 storage buffer 绑定状态缓存，打通 `BufferHandle` → 原生 `MTLBuffer` 句柄解析，并保留读写标志供后续 graphics/compute encoder 消费
- **变更**:
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalPipeline.cs`：实现 `SetStorageBuffers`，按 binding 缓存原生句柄、offset、size 和 `Write` 标志
  - 新增 `TryGetStorageBufferBinding`，供后续 `Draw/DrawIndexed` 与 `DispatchCompute` 复用绑定状态
  - `PROGRESS.md` / `docs/evidence/P4.3.5-meta.json`：固化任务证据并刷新状态机
- **验证**:
  - `dotnet build src/ryubing/src/Ryujinx.Graphics.Metal/Ryujinx.Graphics.Metal.csproj` ✅（85 个既有 CA1416 平台警告，0 错误）
  - `python3 tools/gen_next_task.py` ✅
  - `python3 tools/verify_progress.py` ✅（76/144 任务完成）
- **下一任务**: P4.3.6 — Draw/DrawIndexed: MTLRenderCommandEncoder 绘制

### 2026-06-14 | P4.3.6 Draw/DrawIndexed — MTLRenderCommandEncoder 绘制 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 打通最小 render draw 链路：`queue -> command buffer -> render encoder -> draw -> commit/wait`
- **变更**:
  - `src/libmetal_bridge/include/metal_bridge.h` / `include/metal_internal.h`：新增 command buffer / render encoder / draw C ABI 与内部句柄结构
  - `src/libmetal_bridge/src/MetalCommandBuffer.cpp`：实现最小 command buffer、内部临时 `1x1 BGRA8Unorm` 颜色附件、render encoder 绑定与 `Draw/DrawIndexed`
  - `src/libmetal_bridge/src/MetalDevice.cpp`：补 `metal_release` 对 command buffer / render encoder 的释放分发
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalNative.cs`：补齐 draw ABI P/Invoke 与 primitive/index 枚举
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalRenderer.cs` / `MetalPipeline.cs`：接入 queue 句柄、primitive/index/vertex buffer 查询与当前绑定状态下发
- **验证**:
  - `cmake -S src/libmetal_bridge -B build/libmetal_bridge && cmake --build build/libmetal_bridge --target metal_bridge -j4` ✅
  - `dotnet build src/ryubing/src/Ryujinx.Graphics.Metal/Ryujinx.Graphics.Metal.csproj` ✅（107 个既有 CA1416 平台警告，0 错误）
- **说明**:
  - 当前 render encoder 使用内部临时 `1x1` 颜色附件，仅用于打通 `P4.3.6` 的真实 draw 路径
  - 下一任务 `P4.3.7` 将把该临时附件替换为 `SetRenderTargets` 驱动的真实 `MTLRenderPassDescriptor`
- **下一任务**: P4.3.7 — SetRenderTargets: MTLRenderPassDescriptor
