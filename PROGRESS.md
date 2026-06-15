# 项目进度账本
# 最后更新: 2026-06-15T14:41:07+08:00
# 当前阶段: Phase 5 — 命令映射与状态跟踪
# 完成度: 108/165 任务 (65.5%)
# 仓库: switch-metal-backend

# ===================================================================
## Phase 4: 核心 Metal 后端实现与 2D 闭环重排
### 阶段状态: 🔄 IN PROGRESS
### 出口: 2D 最小闭环稳定跑通，且轻量 2D homebrew 可渲染

## ── 图例 ──
## ⬜ 待开始   🔄 进行中   ✅ 已完成   🚫 已阻塞   ⏸️ 已跳过

# ===================================================================
## Phase 0: 开发环境与工具链搭建
### 阶段状态: ✅ COMPLETE (2026-06-11)
# ===================================================================
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P0.1 | 验证本地工具链 (devkitPro+CLT+Metal+MSC) | ✅ | 2026-06-11 | 全部工具路径确认 |
| P0.1a | 验证 MSC 在 CLT-only 环境下工作 | ✅ | 2026-06-11 | DXIL→metallib, CLT SDK 足够 |
| P0.1b | 确认 metal-cpp 头文件位置 | ✅ | 2026-06-11 | MTLDevice.h 在 CLT SDK 中 |
| P0.1c | 记录环境约束到文档 | ✅ | 2026-06-11 | AGENTS.md §6 + architecture.md |
| P0.2 | 验证 slangc DXIL/Metal/SPIR-V 三目标 | ✅ | 2026-06-11 | 全通过; DXIL 需 -profile sm_6_0 |
| P0.3 | MSC 4.0 全阶段验证 (VS/PS/CS) | ✅ | 2026-06-11 | 3132→6056, 2972→5052, 2832→4692 |
| P0.4 | SPIRV-Tools 6 件套验证 | ✅ | 2026-06-11 | 全部在 /opt/homebrew/bin/ |
| P0.5 | glslangValidator v11.16.3.0 | ✅ | 2026-06-11 | GLSL→SPIR-V 820B 成功 |
| P0.6 | Rust 1.95.0 + cargo | ✅ | 2026-06-11 | 版本确认 |
| P0.7 | Shader Playground 验证 | ✅ | 2026-06-11 | 网站离线, 本地验证替代 |
| P0.8 | 克隆 Ryubing + dotnet build | ✅ | 2026-06-11 | ~/dev/ryubing/; build 22.5s, 16w |
| P0.9 | 克隆 dxmt + 提取模块 | ✅ | 2026-06-11 | 3Shain/dxmt v0.80; 模块在 src/dxmt/ |
| P0.10 | Git 仓库 + 目录结构 | ✅ | 2026-06-11 | 完整仓库模板就绪 |
| P0.11 | tools-verify.sh 编写 | ✅ | 2026-06-11 | 15 项工具检查 + Path A 端到端 |
| P0.12 | VS Code 配置 | ✅ | 2026-06-11 | 4 文件配置完成 |

### 阶段总结
- **关键发现**: MSC 在 CLT-only 完全可用; Path A 为主路径
- **不可用路径**: Path B/C 需 xcrun metal (完整 Xcode)，暂非阻塞
- **参考仓库**: Ryubing @ ~/dev/ryubing/; dxmt @ ~/dev/dxmt/

# ===================================================================
## Phase 1: 着色器管道概念验证
### 阶段状态: ✅ COMPLETE (2026-06-12)
### 出口: ≥3 条路径验证通过 + ≥3 个真实游戏着色器可编译
# ===================================================================
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P1.1 | 编写 test_slang_dxil.sh (Path A 端到端) | ✅ | 2026-06-12T00:25:47+08:00 | tools/test_slang_dxil.sh 4/4 PASS |
| P1.2 | 编写 test_glslang_spirv.sh (Path C SPIR-V) | ✅ | 2026-06-12T00:32:21+08:00 | tools/test_glslang_spirv.sh 8/8 PASS |
| P1.3 | 编写 test_slang_spirv.sh (Slang→SPIR-V) | ✅ | 2026-06-12T00:40:20+08:00 | tools/test_slang_spirv.sh 8/8 PASS |
| P1.4 | 编写 test_spirv_cross_msl.sh (Path C 端到端) | ✅ | 2026-06-12T00:44:10+08:00 | tools/test_spirv_cross_msl.sh 10/10 PASS |
| P1.5 | 编写 test_slang_metal.sh (Path B 尝试) | ✅ | 2026-06-12T00:55:35+08:00 | tools/test_slang_metal.sh 4/4 PASS + 2 预期 SKIP |
| P1.6 | 真实游戏顶点着色器 Path A 测试（龙神缓存 + deko3d 示例 + 内嵌特性覆盖，目标：管线不崩） | ✅ | 2026-06-12T01:31:04+08:00 | tools/test_real_vs_path_a.sh 11 PASS + 3 预期失败 + 5 MSL 跳过，新增发现 gl_PointSize 不兼容 |
| P1.7 | 真实游戏片段着色器 Path A 测试（龙神缓存 + deko3d 示例，目标：管线不崩） | ✅ | 2026-06-12T02:52:08+08:00 | docs/evidence/P1.7-real-fs-path-a.log：11 Path A 通过 + 0 失败 + 6 MSL 跳过，新增发现 FS 固定 ps_6_0 |
| P1.8 | 真实游戏计算着色器 Path A 测试 | ✅ | 2026-06-12T03:22:52+08:00 | docs/evidence/P1.8-real-cs-path-a.log：7 Path A 通过 + 0 失败 + 3 侦察/预期问题，Ryujinx 缓存未发现 compute |
| P1.8b | 扩展真实/近真实着色器语料回归测试 | ✅ | 2026-06-12T03:38:21+08:00 | docs/evidence/P1.8b-shader-corpus-path-a.log：29 Path A 通过 + 13 语料发现问题 + 1 预期问题，覆盖 VS/FS/CS |
| P1.9 | bench_compile.sh 基准测试脚本 | ✅ | 2026-06-12 | docs/evidence/bench-20260612-040600.json |

# ===================================================================
## Phase 2: 渐进式渲染 Demo
### 阶段状态: ✅ COMPLETED (19/19 完成，D8 离屏 1098 FPS，超目标 18×)
### 出口: D8 复杂 Demo 在 M1 上 ≥60fps，且 D1-D8 均有构建、运行和证据产物
# ===================================================================
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P2.0 | 收口 P2 规格：同步 D1-D8 验收标准、构建入口和证据格式 | ✅ | 2026-06-12T05:14:30+08:00 | docs/evidence/P2.0-build-entry.txt + docs/p2-demo-evidence.md |
| P2.1 | D1 Hello Triangle：metal-cpp + 手写 MSL + 彩色三角形 | ✅ | 2026-06-12T05:27:30+08:00 | docs/evidence/P2.1-build.txt |
| P2.2 | D1 运行验证：M1 截图或帧缓冲证据 + 构建日志 | ✅ | 2026-06-12T05:39:10+08:00 | docs/evidence/P2.2-run.txt + docs/evidence/P2.2-d1-triangle.png + docs/evidence/P2.2-meta.json |
| P2.3 | D2 Textured Quad：Path A Slang→DXIL→MSC→metallib + 纹理采样 | ✅ | 2026-06-12T05:48:10+08:00 | docs/evidence/P2.3-build.txt |
| P2.4 | D2 运行验证：纹理 quad 截图 + metallib 加载日志 | ✅ | 2026-06-12T06:01:00+08:00 | docs/evidence/P2.4-run.txt + docs/evidence/P2.4-d2-textured-quad.png + docs/evidence/P2.4-meta.json + docs/evidence/P2.4-fragment-reflection.json |
| P2.5 | D3 Multi-Texture：多纹理混合 + 采样器 filter/wrap/mipmap 模式 | ✅ | 2026-06-12T06:10:29+08:00 | docs/evidence/P2.5-run.txt + docs/evidence/P2.5-d3-multi-texture.png + docs/evidence/P2.5-meta.json + docs/evidence/P2.5-fragment-reflection.json |
| P2.6 | D4 Basic Lighting：uniform buffer + 3D 变换 + 深度测试 + Phong 光照 | ✅ | 2026-06-12T07:30:00+08:00 | docs/evidence/P2.6-run.txt + docs/evidence/P2.6-d4-basic-lighting.png + docs/evidence/P2.6-meta.json |
| P2.6a | Demo 预览窗口：NSWindow + CAMetalLayer，本地可视预览 D1-D4，保留离屏证据链 | ✅ | 2026-06-12T15:39:55+08:00 | docs/evidence/P2.6a-window.txt + docs/evidence/P2.6a-preview-grid.png + docs/evidence/P2.6a-meta.json |
| P2.6b | D4 实时窗口版：自动旋转立方体，本地可视验证 3D 变换与光照链路 | ✅ | 2026-06-12T16:06:01+08:00 | docs/evidence/P2.6b-window.txt + docs/evidence/P2.6b-d4-rotating-window.png + docs/evidence/P2.6b-meta.json |
| P2.7 | D5 Advanced Texturing：法线贴图 + Cubemap/Skybox + RTT 或 MSAA 子集 | ✅ | 2026-06-12T16:33:41+08:00 | docs/evidence/P2.7-run.txt + docs/evidence/P2.7-d5-advanced-texturing.png + docs/evidence/P2.7-meta.json |
| P2.7a | D5 展示增强：更直观的天空盒与法线贴图，并补充 MSAA 对比证据 | ✅ | 2026-06-12T21:59:18+08:00 | docs/evidence/P2.7a-run.txt + docs/evidence/P2.7a-d5-showcase.png + docs/evidence/P2.7a-d5-msaa-compare.png + docs/evidence/P2.7a-meta.json |
| P2.8 | D6 Advanced Lighting：Shadow Map + HDR/Tone Mapping + Bloom 后处理 | ✅ | 2026-06-12T22:18:25+08:00 | docs/evidence/P2.8-run.txt + docs/evidence/P2.8-d6-advanced-lighting.png + docs/evidence/P2.8-meta.json |
| P2.8a | D6 主路径桥接：Shadow/Lighting Pass 切换到 Slang→DXIL→MSC→metallib，后处理保留手写 MSL | ✅ | 2026-06-12T22:52:12+08:00 | docs/evidence/P2.8a-run.txt + docs/evidence/P2.8a-d6-patha-advanced-lighting.png + docs/evidence/P2.8a-meta.json + docs/evidence/P2.8a-scene-fragment-reflection.json |
| P2.8b | D6 双路径对照：Path A 与手写 MSL 输出一致性验证 + 差异证据 | ✅ | 2026-06-12T23:07:24+08:00 | docs/evidence/P2.8b-d6-patha.png + docs/evidence/P2.8b-d6-legacy-msl.png + docs/evidence/P2.8b-d6-diff-heatmap.png + docs/evidence/P2.8b-compare.txt + docs/evidence/P2.8b-meta.json |
| P2.8c | D6 高风险语义回归：uniform/matrix/depth compare/HDR attachment/sampler compare 证据固化 | ✅ | 2026-06-12T23:11:18+08:00 | docs/evidence/P2.8c-run.txt + docs/evidence/P2.8c-semantics.txt + docs/evidence/P2.8c-meta.json |
| P2.9 | D7 GPU-Driven：Compute 粒子 + Instancing + Indirect Draw | ✅ | 2026-06-12T23:42:42+08:00 | docs/evidence/P2.9-run.txt + docs/evidence/P2.9-d7-gpu-driven.png + docs/evidence/P2.9-meta.json + docs/evidence/P2.9-perf.json + docs/evidence/P2.9-compute-reflection.json |
| P2.10 | D8 Complex Showcase：PBR 材质球 + 阴影 + 天空盒 + 后处理 + 粒子 + HUD + 自由摄像机 | ✅ | 2026-06-13T01:35:00+08:00 | docs/evidence/P2.10-d8-complex-showcase.png + docs/evidence/P2.10-run.txt + docs/evidence/P2.10-meta.json + docs/evidence/P2.10-perf.json：7 个 PBR 球体 (GGX) + shadow map + 程序化天空盒 + bloom + 粒子漩涡 + HUD + 自由轨道摄像机，M1 上 812 FPS |
| P2.11 | D8 性能验证：M1 上 ≥60fps 或记录阻塞瓶颈 | ✅ | 2026-06-13T02:45:00+08:00 | docs/evidence/P2.11-run.txt + docs/evidence/P2.11-perf.json + docs/evidence/P2.11-d8-perf.png + docs/evidence/P2.11-meta.json：离屏 1098 FPS (0.91ms/帧)，窗口 ~59 FPS (vsync 60Hz 限流)，GPU 渲染性能 18x 超过目标 |
| P2.12 | Demo 构建与回归入口：make build-demos + 截图/JSON 证据规范 | ✅ | 2026-06-13T02:48:00+08:00 | docs/evidence/P2.12-build.txt + docs/evidence/P2.12-meta.json：D1–D8 全部成功构建，新增 evidence/evidence-d8 顶层目标，统一 build-demos 入口覆盖所有 Demo |

# ===================================================================
## Phase 3: Ryubing Fork 与 GAL 集成
### 阶段状态: ⬜ PENDING
### 出口: Ryujinx.Graphics.Metal 编译通过
# ===================================================================
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P3.0 | 基于 kk 报告收口后续任务拆分与风险清单 | ✅ | 2026-06-13T12:11:05+08:00 | docs/kk-task-expansion.md |
| P3.1 | Fork Ryubing + feature/native-metal-backend 分支 | ✅ | 2026-06-13T12:28:00+08:00 | 代码已存在 src/ryubing |
| P3.1a | 收口 libmetal_bridge 模块骨架 + C ABI/opaque handle 方案 | ✅ | 2026-06-13T12:36:00+08:00 | docs/p3-libmetal-bridge-abi.md |
| P3.1b | 收口 MetalShaderCompiler 单例 + workaround 位掩码设计 | ✅ | 2026-06-13T12:45:00+08:00 | docs/p3-shader-compiler-design.md |
| P3.2 | 创建 Ryujinx.Graphics.Metal 项目 + .csproj | ✅ | 2026-06-13T12:54:00+08:00 | 代码已存在 |
| P3.3 | 引用 Ryujinx.Graphics.GAL 和 Shader 依赖 | ✅ | 2026-06-13T13:00:22+08:00 | 代码已存在 |
| P3.4 | 创建 MetalNative.cs (P/Invoke 声明) | ✅ | 2026-06-13T13:03:22+08:00 | 代码已存在 |
| P3.5 | 创建 MetalRenderer.cs (IRenderer stub) | ✅ | 2026-06-13T13:05:33+08:00 | 代码已存在 |
| P3.6 | 创建 MetalPipeline.cs (IPipeline 63方法 stub) | ✅ | 2026-06-13T13:14:29+08:00 | 代码已存在 |
| P3.7 | 创建 MetalDevice.cs (MTLDevice 管理) | ✅ | 2026-06-13T13:17:29+08:00 | 代码已存在 |
| P3.8 | 创建 MetalShaderCompiler.cs (Slang+MSC 封装) | ✅ | 2026-06-13T13:23:45+08:00 | 代码已存在 |
| P3.9 | 创建 MetalBuffer/Texture/Sampler stubs | ✅ | 2026-06-13T13:28:00+08:00 | 代码已存在 |
| P3.10 | 修改启动代码注册 Metal 后端选项 | ✅ | 2026-06-13T14:05:00+08:00 | 代码已存在 |
| P3.11 | 验证：选择 Metal 后端→启动→空白窗口不崩溃 | ✅ | 2026-06-13T14:20:00+08:00 | docs/evidence/P3.11-metal-backend-launch-test.log |

# ===================================================================
## Phase 4: 核心 Metal 后端实现与 2D 闭环重排
### 阶段状态: 🔄 IN PROGRESS
### 出口: 2D 最小闭环稳定跑通，且轻量 2D homebrew 可渲染
# ===================================================================
### 4.1 设备与资源管理
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P4.1.0 | 固化 Metal 硬件限制常量与资源对齐策略 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.1.1 | MetalDevice: GPU 选择 + MTLDevice 创建 + 特性查询 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.1.2 | MetalBuffer: MTLStorageMode 策略 (Managed/Private/Shared) | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.1.3 | MetalTexture: Maxwell→MTLPixelFormat 映射表 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.1.4 | MetalSampler: 过滤/包裹/比较模式映射 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.1.5 | 稀疏缓冲区 CreateBufferSparse: MTLHeap+MTLBuffer | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.1.6 | GetCapabilities/HardwareInfo 查询实现 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |

### 4.2 着色器编译器集成
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P4.2.0 | 建立 MSC/Metal 限制验证矩阵 (纹理/subgroup/discard/helper) | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.2.1 | CreateProgram: Source→MTLLibrary | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.2.2 | Slang API P/Invoke: Slang 原生语法→DXIL | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.2.3 | libmetalirconverter P/Invoke: DXIL→metallib | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.2.4 | 磁盘着色器缓存 (~/Library/Caches/SwitchMetal/) | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.2.5 | 回退逻辑: Path A→Path C→Path B | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.2.6 | LoadProgramBinary: 加载缓存 metallib | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |

### 4.3 管线状态与绘制
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P4.3.1 | SetProgram → MTLRenderPipelineState 创建 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.2 | SetVertexBuffers/SetVertexAttribs: 顶点布局映射 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.3 | SetUniformBuffers: MTLBuffer 绑定 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.4 | SetTextureAndSampler: 纹理+采样器绑定 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.5 | SetStorageBuffers: Compute/Graphics 存储缓冲 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.6 | Draw/DrawIndexed: MTLRenderCommandEncoder 绘制 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.7 | SetRenderTargets: MTLRenderPassDescriptor | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.8 | ClearRenderTarget: 清屏操作 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.9 | SetBlendState: 混合状态映射 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.10 | SetDepthTest/SetStencilTest: DepthStencilState | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.11 | SetScissors/SetViewports: 视口+裁剪 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.3.12 | SetFaceCulling/SetFrontFace/SetPolygonMode | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |

### 4.4 同步与呈现
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P4.4.1 | CommandBuffer 提交+等待: commit+waitUntilCompleted | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.4.2 | CreateSync/WaitSync: MTLEvent 信号量 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.4.3 | Presenter/Window: CAMetalLayer + 交换链 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.4.4 | ScreenCaptured 事件: 帧缓冲→CGImage | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.4.5 | BackgroundContextAction: 后台 MTLCommandQueue | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |
| P4.4.6 | RunLoop: 主渲染循环 | ✅ | 2026-06-14T14:48:46+08:00 | 参见 Phase 4 阶段总结 |

### 4.5 既有模块验收
### 出口验证: Phase 4 核心模块已完成，但不再视为“真实 2D 游戏闭环已证明”

> **重排说明**：P4.5 的通过说明底层模块可工作，不等于真实 2D 游戏链路已经收敛。
> 从本轮重排开始，真实 2D 验收转移到 `P4.6`，按“手写最小样本 → 轻量 homebrew → 蔚蓝回归”递进。

| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P4.5.1 | 设备创建冒烟测试: MetalDevice 创建 + 特性查询有效 | ✅ | 2026-06-14 | docs/evidence/P4.5.1-meta.json |
| P4.5.2 | 着色器编译验收: ≥5 测试着色器 Path A 编译通过 | ✅ | 2026-06-14 | docs/evidence/P4.5.2-meta.json |
| P4.5.3 | 资源生命周期验收: Buffer/Texture 创建→使用→销毁 100 次无泄漏 | ✅ | 2026-06-14 | docs/evidence/P4.5.3-meta.json |
| P4.5.4 | 基础 Draw 验收: Triangle + 纹理四边形绘制正确 | ✅ | 2026-06-14 | docs/evidence/P4.5.4-meta.json |
| P4.5.5 | 管线状态验收: Blend/Depth/Stencil/Scissor 状态切换正确 | ✅ | 2026-06-14 | docs/evidence/P4.5.5-meta.json |
| P4.5.6 | 同步验收: CommandBuffer 提交+等待+信号量 顺序正确 | ✅ | 2026-06-14 | docs/evidence/P4.5.6-meta.json |
| P4.5.7 | 呈现验收: CAMetalLayer 交换链正常刷新 (≥60fps 空场景) | ✅ | 2026-06-14 | docs/evidence/P4.5.7-meta.json |
| P4.5.8 | 2D Demo 完整渲染: Demo D1 帧缓冲对比 (像素差异<1%) | ✅ | 2026-06-14 | docs/evidence/P4.5.8-meta.json |

### 4.6 2D 主线重排与最小闭环
### 出口验证: Slang 原生主路径收口，手写 2D 样本跑通，至少 2 个轻量 2D homebrew 可渲染

| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P4.6.1 | 收口 2D 调试主线：冻结蔚蓝为后验回归样本，确定手写/轻量 homebrew/复杂样本三级验收顺序 | ✅ | 2026-06-15T12:09:46+08:00 | docs/p4-2d-realignment-plan.md |
| P4.6.2 | 建立 Slang 原生图形着色器模板集：VS/FS 的 ConstantBuffer、纹理采样、varying 语义、alpha blend 最小样本 | ✅ | 2026-06-15T12:18:00+08:00 | docs/evidence/P4.6.2-slang-graphics-templates.log + docs/evidence/P4.6.2-meta.json + docs/p4-slang-graphics-templates.md |
| P4.6.3 | 收紧 Metal 主编译路径：主线禁用 GLSL 直通 Path A，GLSL 桥接降级为诊断/对照模式 | ✅ | 2026-06-15T12:23:18+08:00 | docs/evidence/P4.6.3-libmetal-bridge-build.log + docs/evidence/P4.6.3-metal-build.log |
| P4.6.4 | 建立着色器诊断证据链：源码 dump、Slang 输出、DXIL 摘要、metallib 产物、失败归因标签 | ✅ | 2026-06-15T12:31:00+08:00 | docs/evidence/P4.6.4-diagnostic-chain.log + docs/evidence/P4.6.4-meta.json + docs/evidence/P4.6.4-diagnostics/index.json |
| P4.6.5 | 建立渲染诊断证据链：RenderTarget dump、Draw 次序日志、关键状态快照、Present 前后帧抓取 | ✅ | 2026-06-15T12:35:25+08:00 | docs/p4-render-diagnostic-evidence.md + docs/evidence/P4.6.5-diagnostic-bundle/ + docs/evidence/P4.6.5-render-diagnostic.log + docs/evidence/P4.6.5-meta.json |
| P4.6.6 | 实现手写 2D 最小样本 A：单纹理 Quad + Sprite Atlas + alpha blend | ✅ | 2026-06-15T12:53:50+08:00 | docs/p4-handwritten-2d-sample-a.md + src/demos/p4_6a/ + docs/evidence/P4.6.6-p4-6a-sprite-quad.ppm + docs/evidence/P4.6.6-p4-6a-sprite-atlas.ppm + docs/evidence/P4.6.6-run.txt + docs/evidence/P4.6.6-meta.json |
| P4.6.7 | 实现手写 2D 最小样本 B：Tile Map + camera scroll + HUD 文本/面板 | ✅ | 2026-06-15T13:08:40+08:00 | docs/p4-handwritten-2d-sample-b.md + src/demos/p4_6b/ + docs/evidence/P4.6.7-p4-6b-tilemap-scroll-a.ppm + docs/evidence/P4.6.7-p4-6b-tilemap-scroll-b.ppm + docs/evidence/P4.6.7-p4-6b-tilemap-atlas.ppm + docs/evidence/P4.6.7-run.txt + docs/evidence/P4.6.7-meta.json |
| P4.6.8 | 轻量 2D homebrew smoke：打通 `TetrisNX`/`Pong-NX` 级别样本并记录首帧证据 | ✅ | 2026-06-15T13:22:13+08:00 | docs/p4-lightweight-homebrew-smoke.md + src/demos/p4_6c/ + docs/evidence/P4.6.8-p4-6c-homebrew-smoke.ppm + docs/evidence/P4.6.8-p4-6c-homebrew-atlas.ppm + docs/evidence/P4.6.8-run.txt + docs/evidence/P4.6.8-meta.json |
| P4.6.9 | 标准 2D homebrew 验收：打通 `OpenSupaplex` 或 `NXEngine-evo` 之一 | ✅ | 2026-06-15T13:59:01+08:00 | docs/evidence/P4.6.9-run.txt + docs/evidence/P4.6.9-meta.json |
| P4.6.10 | 窗口版 2D smoke：用现成 NSWindow + CAMetalLayer + Presenter 呈现 smoke 首帧 | ✅ | 2026-06-15T14:21:49+08:00 | docs/evidence/P4.6.10-run.txt + docs/evidence/P4.6.10-window-smoke.ppm + docs/evidence/P4.6.10-window-smoke.png + docs/evidence/P4.6.10-meta.json |

### 阶段总结
- **已完成模块**: 设备管理 (P4.1)、着色器编译 (P4.2)、管线状态 (P4.3)、同步与呈现 (P4.4)
- **新增主线**: 先收口 Slang 原生主路径，再建立最小 2D 闭环，最后回到蔚蓝；P4.6.10 先改为窗口版 smoke，优先验证 Presenter/窗口管道
- **风险拆分**: Shader / Pipeline / Bridge 三类问题分流
- **下一阶段前置条件**: P4.6 完成后，再进入 Phase 5 的 Maxwell/GAL 状态映射扩展


# ===================================================================
## Phase 5: 命令映射与状态跟踪
### 阶段状态: ⬜ PENDING
### 出口: 3D homebrew 可渲染
# ===================================================================
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P5.0 | 搭建 Maxwell/GAL→Metal 状态映射表骨架 | ⬜ | — | — |
| P5.1 | Maxwell→Metal 状态映射表 (deko3d+envytools) | ⬜ | — | — |
| P5.2 | NVN SetBlendState → Metal blend state | ⬜ | — | — |
| P5.3 | NVN SetDepthStencilState → Metal depth/stencil | ⬜ | — | — |
| P5.4 | NVN SetRasterizerState → Metal rasterizer | ⬜ | — | — |
| P5.5 | NVN SetVertexArrayState → Metal vertex descriptor | ⬜ | — | — |
| P5.6 | NVN SetViewport/SetScissor → Metal viewport/scissor | ⬜ | — | — |
| P5.7 | DispatchCompute: MTLComputeCommandEncoder | ⬜ | — | — |
| P5.8 | SetStorageBuffers + SetImage: Compute 资源绑定 | ⬜ | — | — |
| P5.9 | CopyBuffer: MTLBlitCommandEncoder 数据拷贝 | ⬜ | — | — |
| P5.10 | 纹理数据上传/下载: Buffer↔Texture | ⬜ | — | — |
| P5.11 | 几何着色器路径: Maxwell GS→Vertex+Compute 解构 | ⬜ | — | — |
| P5.12 | 曲面细分路径: Maxwell Tess→Compute+Post-TCS | ⬜ | — | — |
| P5.13 | Transform Feedback → MTLBuffer 写入 | ⬜ | — | — |
| P5.14 | Indirect Draw + Conditional Rendering | ⬜ | — | — |

# ===================================================================
## Phase 6: 测试与验证体系
### 阶段状态: ⬜ PENDING
### 出口: 50+ 测试用例通过，CI 绿色
# ===================================================================
### 6.1 单元测试
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P6.1.1 | 单元测试框架搭建 (Catch2 + xUnit) | ⬜ | — | — |
| P6.1.2 | 格式映射表全覆盖测试 | ⬜ | — | — |
| P6.1.3 | 着色器解码单元测试 (≥10) | ⬜ | — | — |
| P6.1.4 | 像素格式映射测试 | ⬜ | — | — |

### 6.2 着色器编译集成测试
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P6.2.0 | 将 kk Metal 限制清单转为着色器编译回归样本 | ⬜ | — | — |
| P6.2.1 | Slang 原生 Path A 端到端编译 (10 个图形/计算样本) | ⬜ | — | — |
| P6.2.2 | GLSL 桥接诊断回归 (仅对照，不作为主成功路径) | ⬜ | — | — |
| P6.2.3 | 回退逻辑验证 | ⬜ | — | — |
| P6.2.4 | 着色器缓存命中测试 | ⬜ | — | — |
| P6.2.5 | 跨阶段优化验证 (spirv-opt) | ⬜ | — | — |

### 6.3 Metal 管线集成测试
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P6.3.1 | 基础 Draw 调用 | ⬜ | — | — |
| P6.3.2 | 纹理 Draw 调用 (像素差异<1%) | ⬜ | — | — |
| P6.3.3 | MRT 渲染 (3附件) | ⬜ | — | — |
| P6.3.4 | Compute Dispatch | ⬜ | — | — |
| P6.3.5 | 同步正确性 | ⬜ | — | — |
| P6.3.6 | 2D 样本分层回归：手写样本 / smoke homebrew / 标准 homebrew | ⬜ | — | — |

### 6.4 回归测试 + CI
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P6.4.1 | Demo D1-D5 帧缓冲对比 | ⬜ | — | — |
| P6.4.2 | Game Shader 编译回归 (20个着色器) | ⬜ | — | — |
| P6.4.3 | 性能基准 (D8 ≤16ms 帧时间) | ⬜ | — | — |
| P6.4.4 | CI 流水线: GitHub Actions 自动化测试 | ⬜ | — | — |
| P6.4.5 | 蔚蓝回归脚本：首帧、首屏、切场景三段证据采集 | ⬜ | — | — |

# ===================================================================
## Phase 7: 性能优化与 MetalFX
### 阶段状态: ⬜ PENDING
### 出口: D8≥60fps, 3D≥30fps
# ===================================================================
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P7.1 | MetalFX Upscaling: 720p→1440p/4K | ⬜ | — | — |
| P7.2 | MetalFX Frame Interpolation: 30→60fps | ⬜ | — | — |
| P7.3 | GPU Trace 性能剖析: 定位瓶颈 | ⬜ | — | — |
| P7.4 | Argument Buffers 优化: 批量资源绑定 | ⬜ | — | — |
| P7.5 | 着色器编译预热: 启动时预编译 | ⬜ | — | — |
| P7.6 | 异步编译管线 (DXVK 模式) | ⬜ | — | — |
| P7.7 | 内存优化: MTLStorageMode 策略调优 | ⬜ | — | — |

# ===================================================================
## Phase 8: 兼容性修复与打磨
### 阶段状态: ⬜ PENDING
### 出口: Top 20 游戏 ≥15 可玩
# ===================================================================
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P8.1 | Game Compatibility Tracker (CSV/Google Sheets) | ⬜ | — | — |
| P8.2 | 逐游戏测试: 可启动→可渲染→可玩→完美 | ⬜ | — | — |
| P8.3 | 崩溃修复 (最高优先级) | ⬜ | — | — |
| P8.4 | 渲染错误修复 | ⬜ | — | — |
| P8.5 | 几何着色器兼容修复 | ⬜ | — | — |
| P8.6 | 曲面细分兼容修复 | ⬜ | — | — |
| P8.7 | 像素格式边缘情况 | ⬜ | — | — |
| P8.8 | 纹理压缩: ASTC/BCn/ETC2 映射 | ⬜ | — | — |

# ===================================================================
## Phase 9: 发布准备
### 阶段状态: ⬜ PENDING
### 出口: Public Beta 发布
# ===================================================================
| ID | 任务 | 状态 | 完成时间 | 证据 |
|----|------|------|----------|------|
| P9.1 | macOS 原生启动器 (SwiftUI + Metal 后端选择) | ⬜ | — | — |
| P9.2 | CI/CD 完整流水线 (build+test+release) | ⬜ | — | — |
| P9.3 | 用户文档: 安装指南 + 游戏兼容列表 + FAQ | ⬜ | — | — |
| P9.4 | 开发者文档: 架构说明 + 贡献指南 + API 文档 | ⬜ | — | — |
| P9.5 | libmetalirconverter 许可确认 (可否随发行版分发) | ⬜ | — | — |
| P9.6 | Public Beta 发布 + 反馈收集渠道 | ⬜ | — | — |

# ===================================================================
## ── 阻塞项 ──
# ===================================================================
| ID | 阻塞项 | 影响任务 | 解决方案 | 状态 |
|----|--------|----------|----------|------|
| — | (暂无) | — | — | — |

# ===================================================================
## ── 统计 ──
# ===================================================================
- 总任务数: 165
- 已完成: 108 (65.5%)
- 进行中: 0
- 阻塞: 0
- 跳过: 0
- 待开始: 57
- 当前阶段: Phase 5 — 命令映射与状态跟踪
- 下一任务: P5.0 — 搭建 Maxwell/GAL→Metal 状态映射表骨架
