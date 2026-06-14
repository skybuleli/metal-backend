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
- 当前进度：78/144 任务完成
- 下一任务：P4.3.8 — ClearRenderTarget: 清屏操作
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

### 2026-06-14 | P4.3.7 SetRenderTargets — MTLRenderPassDescriptor | ✅ 完成

- **Agent**: Qoder
- **结果**: ✅ 实现真实渲染目标 MTLRenderPassDescriptor 创建，替换 P4.3.6 临时 1x1 颜色附件
- **变更**:
  - `src/libmetal_bridge/include/metal_bridge.h`：新增 `metal_load_action`/`metal_store_action` 枚举、`metal_clear_color`/`metal_clear_depth_stencil` 结构体、`metal_color_attachment_descriptor`/`metal_depth_stencil_attachment_descriptor` 描述符、`metal_begin_render_encoding_with_targets` C ABI
  - `src/libmetal_bridge/include/metal_internal.h`：更新 `metal_render_encoder` 为 `color_targets[8]` 数组 + `depth_stencil_target` + `color_target_count`
  - `src/libmetal_bridge/src/MetalCommandBuffer.cpp`：实现 `metal_begin_render_encoding_with_targets`（MTLRenderPassDescriptor 配置多颜色附件 + 深度/模板附件 + 纹理引用保留）
  - `src/libmetal_bridge/src/MetalDevice.cpp`：更新 `metal_release` METAL_HANDLE_TYPE_RENDER_ENCODER 释放所有颜色/深度附件
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalNative.cs`：新增 P/Invoke 类型 + `BeginRenderEncodingWithTargets`（含深度/无深度两个便捷重载）
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalPipeline.cs`：实现 `SetRenderTargets`、新增 `MetalRenderTargetState`、修改 `ExecuteRenderDraw` 路由
- **验证**:
  - `cmake --build src/libmetal_bridge/build` ✅（0 警告，0 错误）
  - `dotnet build src/ryubing/src/Ryujinx.Graphics.Metal/Ryujinx.Graphics.Metal.csproj` ✅（109 个既有 CA1416 警告，0 错误）
- **说明**:
  - 最多 8 颜色附件，通过 `pixel_format` 判断深度/模板格式
  - 保留 `metal_begin_render_encoding` 作为回退路径（无渲染目标时）
  - 默认 `LoadAction::Load + StoreAction::Store` 保留已有渲染内容
- **下一任务**: P4.3.8 — ClearRenderTarget: 清屏操作

---

### 2026-06-14 11:15 — P4.3.8 ClearRenderTarget: 清屏操作
- **Agent**: Qoder
- **结果**: ✅ 实现颜色/深度/模板清除缓存机制，通过 MTLRenderPassDescriptor loadAction=Clear 完成清屏
- **变更**:
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalPipeline.cs`：实现 `ClearRenderTargetColor` + `ClearRenderTargetDepthStencil`；`MetalRenderTargetState` 新增 `PendingColorClear`/`PendingDepthStencilClear` 缓存结构；`BuildColorDescriptors`/`BuildDepthStencilDescriptor` 支持 LoadAction.Clear；`ExecuteRenderDraw` 渲染通道开始后调用 `ClearPending()`
- **验证**:
  - `dotnet build src/ryubing/src/Ryujinx.Graphics.Metal/Ryujinx.Graphics.Metal.csproj` ✅（109 个 CA1416 警告，0 错误）
- **说明**:
  - Metal 清屏通过渲染通道描述符的 loadAction=Clear 实现，非通道内清除
  - componentMask 部分清除回退为全通道清除（Metal 限制）
  - 纯 C# 变更，复用 P4.3.7 已有的 C ABI 描述符机制
- **状态摘要**: P4 进度 79/144 (54.9%)，下一任务 P4.3.9 SetBlendState

---

### 2026-06-14 11:45 — P4.3.9 SetBlendState: 混合状态映射
- **Agent**: Qoder
- **结果**: ✅ 实现完整混合状态映射，包含 C ABI 枚举/描述符 + C++ 管线创建应用 + C# 缓存与重建
- **变更**:
  - `src/libmetal_bridge/include/metal_bridge.h`：新增 `metal_blend_factor`/`metal_blend_operation`/`metal_color_write_mask` 枚举 + `metal_blend_attachment_descriptor` + 管线描述符替换 `reserved[2]` 为 `blend_attachments` 指针
  - `src/libmetal_bridge/src/MetalPipeline.cpp`：`metal_create_render_pipeline` 应用混合状态到每个颜色附件
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalNative.cs`：新增 `MetalBlendFactor`/`MetalBlendOperation`/`MetalColorWriteMask` 枚举 + `MetalBlendAttachmentDescriptor` + 描述符更新
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalPipeline.cs`：缓存 `_blendAttachments` + 实现 `SetBlendState` + `ConvertBlendFactor`/`ConvertBlendOp` 映射 + `CreatePipelineFromProgram` 固定混合数组传递
- **验证**:
  - `cmake --build src/libmetal_bridge/build` ✅（0 错误）
  - `dotnet build src/ryubing/src/Ryujinx.Graphics.Metal/Ryujinx.Graphics.Metal.csproj` ✅（109 个 CA1416 警告，0 错误）
- **说明**:
  - Metal 混合状态烘焙在管线创建时，SetBlendState 触发 RecreatePipelineForLayoutChange
  - AdvancedBlendState 回退为标准混合（Metal 不原生支持 KHR_blend_equation_advanced）
  - BlendFactor 枚举值与 MTL::BlendFactor 对齐，支持 GL 后缀变体
- **状态摘要**: P4 进度 80/144 (55.6%)，下一任务 P4.3.10 SetDepthTest/SetStencilState

---

### 2026-06-14 12:15 — P4.3.10 SetDepthTest/SetStencilTest: DepthStencilState
- **Agent**: Qoder
- **结果**: ✅ 实现深度/模板状态创建、动态绑定和释放，包含 C ABI 枚举/描述符/函数 + C++ 实现 + C# 缓存与懒重建
- **变更**:
  - `src/libmetal_bridge/include/metal_bridge.h`：新增 `METAL_HANDLE_TYPE_DEPTH_STENCIL_STATE` + `metal_stencil_operation` 枚举 + `metal_stencil_descriptor` / `metal_depth_stencil_descriptor` 结构体 + 3 个新函数声明
  - `src/libmetal_bridge/include/metal_internal.h`：新增 `metal_depth_stencil_state` 内部结构体
  - `src/libmetal_bridge/src/MetalPipeline.cpp`：实现 `metal_create_depth_stencil_state` + `metal_render_encoder_set_depth_stencil_state` + `metal_render_encoder_set_stencil_reference_value`
  - `src/libmetal_bridge/src/MetalDevice.cpp`：`metal_release` 新增 `DEPTH_STENCIL_STATE` 分支
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalNative.cs`：新增 `MetalStencilOperation` 枚举 + 描述符结构体 + 3 个 P/Invoke
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalPipeline.cs`：`_depthStencilStateHandle` 缓存 + `SetDepthTest`/`SetStencilTest`/`SetDepthMode` + `UpdateDepthStencilState` 脏标志懒重建 + `BindRenderResources` 绑定
- **验证**:
  - `cmake --build src/libmetal_bridge/build` ✅（0 警告，0 错误）
  - `dotnet build src/ryubing/src/Ryujinx.Graphics.Metal/Ryujinx.Graphics.Metal.csproj` ✅（113 个 CA1416 警告，0 错误）
- **说明**:
  - Metal 深度/模板状态通过 MTLDepthStencilState 动态绑定到 render encoder，无需重建管线
  - 使用 `_depthStencilDirty` 标志懒重建，避免每次 draw 创建新对象
  - `metal_compare_function` 复用采样器模块已有枚举
  - `setStencilReferenceValues` 同时设置正反面模板引用值
- **状态摘要**: P4 进度 81/144 (56.3%)，下一任务 P4.3.11 SetScissors/SetViewports

---

### 2026-06-14 12:30 — P4.3.11 SetScissors/SetViewports: 视口+裁剪
- **Agent**: Qoder
- **结果**: ✅ 实现视口和裁剪矩形设置，支持多视口/多裁剪，包含 Vulkan→Metal Y 轴坐标翻转
- **变更**:
  - `src/libmetal_bridge/include/metal_bridge.h`：新增 `metal_viewport` + `metal_scissor_rect` 描述符 + `metal_render_encoder_set_viewports` / `metal_render_encoder_set_scissor_rects` 函数声明
  - `src/libmetal_bridge/src/MetalPipeline.cpp`：实现单个 + 多个视口/裁剪矩形设置，修正 metal-cpp 参数顺序 `(data, count)`
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalNative.cs`：新增 `MetalViewport` / `MetalScissorRect` 结构体 + 2 个 P/Invoke
  - `src/ryubing/src/Ryujinx.Graphics.Metal/MetalPipeline.cs`：`MaxViewports=16` + `_viewports`/`_scissorRects` 缓存 + `SetScissors`/`SetViewports` 实现 + `BindRenderResources` 中 fixed 指针绑定
- **验证**:
  - `cmake --build src/libmetal_bridge/build` ✅（0 警告，0 错误）
  - `dotnet build src/ryubing/src/Ryujinx.Graphics.Metal/Ryujinx.Graphics.Metal.csproj` ✅（115 个 CA1416 警告，0 错误）
- **说明**:
  - Vulkan 视口 Y 轴向下、Metal Y 轴向上，通过 `originY = |height| - regionY - height` 翻转坐标
  - ViewportSwizzle 当前忽略（Metal 无原生等价，需通过顶点着色器修改）
  - metal-cpp `setViewports`/`setScissorRects` 参数顺序为 `(data, count)`
  - 支持最多 16 个视口/裁剪矩形
- **状态摘要**: P4 进度 82/144 (56.9%)，下一任务 P4.3.12 SetFaceCulling/SetFrontFace/SetPolygonMode
