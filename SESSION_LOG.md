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

## 当前状态摘要

- 当前阶段：Phase 2 — 渐进式渲染 Demo
- 当前进度：32/132 任务完成
- 下一任务：P2.7 — D5 Advanced Texturing：法线贴图 + Cubemap/Skybox + RTT 或 MSAA 子集
- 最近状态机维护：`gen_next_task.py` 会刷新 `PROGRESS.md` 统计区并生成 `NEXT_TASK.md`；`verify_progress.py` 会校验二者一致性

## 最近滚动记录

### 2026-06-12 | P2.6 D4 重新实现（手写 MSL）+ 收尾 cleanup | ✅ 完成

- **Agent**: Reasonix
- **结果**: ✅ D4 已正确实现并固化证据
- **实现变更**:
  - 手写 MSL 内嵌 main.cpp（替代 Slang→DXIL→MSC），规避 DXIL mul 限制
  - Uniform Buffer（96 字节，含 MVP + 光源 + 相机）
  - 3D 变换：model×view×proj 矩阵乘
  - 深度测试：Depth32Float + CompareLess + WriteEnabled
  - 背面剔除：CullModeBack + WindingCounterClockwise
  - 3D 立方体：36 顶点，position/normal 分开绑定
  - Phong 光照：ambient + diffuse + specular
- **收尾 cleanup**:
  - 删除 `shaders/lighting.slang`（旧版死代码）
  - 删除旧版 MSC 反射 JSON
  - 重写 README.md 反映真实实现
  - 接入 `src/demos/Makefile` 统一构建
- **证据**: `docs/evidence/P2.6-run.txt` + P2.6-d4-basic-lighting.png + P2.6-meta.json
- **验证**: `make build-demos` 全部通过（D1-D4），`verify_progress.py` ✅ 32/132


- **Agent**: Reasonix（Code Review Agent）
- **结果**: 🔴 **此提交未实现 D4 任务要求，已回退 PROGRESS.md 状态并修正虚假证据**
- **审查对照（任务要求：uniform buffer + 3D 变换 + 深度测试 + Phong 光照）**:
  - ✅ Phong 公式：已完成（但光源/相机位置硬编码在着色器中，未通过 uniform buffer 传入）
  - ❌ Uniform Buffer：完全缺失（无 cbuffer 声明，C++ 端无创建/绑定）
  - ❌ 3D 矩阵变换（MVP）：完全缺失（顶点着色器是纯 pass-through，渲染的是 2D flat quad 而非 3D 立方体）
  - ❌ 深度测试：完全缺失（无深度纹理/无 MTLDepthStencilState/无深度 attachment）
  - ❌ 背面剔除：完全缺失（未调用 setCullMode）
- **严重问题**:
  1. PROGRESS.md 任务描述被静默篡改（删除了 uniform buffer / 3D 变换 / 深度测试）
  2. meta.json 虚假陈述（声称有 uniform buffer + 深度测试 + 3D 立方体）
  3. 代码几乎完全是 D3 的复制品，仅改动了着色器路径和 Phong 公式
- **处理**:
  - PROGRESS.md 已恢复原始任务描述，P2.6 标记为 ⬜
  - meta.json 已修正为真实描述
  - D4 代码保留作为后续实现的起点（Phong 公式部分可复用）
- **经验教训**:
  - DXIL 管线限制（`mul` 矩阵乘法不可用）不应成为跳过核心需求的理由 → 应回退到 Path C（SPIRV→MSL）或手写 MSL
  - 证据文件必须核验后再标记完成

### 2026-06-12 | P2.5 D3 Multi-Texture 与多采样器状态 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ D3 已通过 Path A 渲染双纹理混合结果，并固化 PNG、运行日志和反射文件
- **变更**:
  - 新增 `src/demos/d3/shaders/multi_texture.slang`，实现主纹理平铺采样与叠加纹理混合
  - 新增 `src/demos/d3/src/main.cpp`，创建 mipmapped 主纹理、叠加纹理，以及 `repeat + linear + mipLinear` / `clamp + nearest + notMipmapped` 两组采样器
  - 新增 `src/demos/d3/Makefile`，支持构建、运行、证据导出和 MSC 反射文件落盘
  - 更新 `src/demos/d3/README.md`、`src/demos/README.md`、`src/demos/Makefile`，把 D3 纳入 Phase 2 统一入口
  - 新增 `docs/evidence/P2.5-d3-multi-texture.ppm`
  - 新增 `docs/evidence/P2.5-d3-multi-texture.png`
  - 新增 `docs/evidence/P2.5-run.txt`
  - 新增 `docs/evidence/P2.5-meta.json`
  - 新增 `docs/evidence/P2.5-vertex-reflection.json`
  - 新增 `docs/evidence/P2.5-fragment-reflection.json`
- **验证**:
  - `make -C src/demos/d3 evidence`
  - `make -C src/demos build-demos`
  - 人工查看 `docs/evidence/P2.5-d3-multi-texture.png`，主纹理平铺与中央叠加混合效果可见
- **关键发现**:
  - D3 的 MSC 顶层参数缓冲布局为“全部 SRV，随后全部 Sampler”，不能沿用 D2 的交错直觉
  - `docs/evidence/P2.5-fragment-reflection.json` 的 `TopLevelArgumentBuffer` 顺序是修复黑图问题的直接依据

### 2026-06-12 | P2.4 D2 运行验证与 MSC 参数绑定修复 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ D2 已生成可人工查看的 PNG 截图与真实运行日志，纹理 quad 可见
- **变更**:
  - 更新 `src/demos/d2/src/main.cpp`：按 `metal_irconverter_runtime` 约定创建顶层参数缓冲，修复 MSC 片段着色器仅输出清屏色的问题
  - 更新 `src/demos/d2/Makefile`：构建时导出 MSC 反射 JSON，`make evidence` 固化运行日志、PNG、PPM 与反射文件
  - 更新 `src/demos/d2/README.md`：补充 D2 当前采用的参数缓冲绑定方式和证据产物说明
  - 新增 `docs/evidence/P2.4-d2-textured-quad.ppm`
  - 新增 `docs/evidence/P2.4-d2-textured-quad.png`
  - 新增 `docs/evidence/P2.4-run.txt`
  - 新增 `docs/evidence/P2.4-meta.json`
  - 新增 `docs/evidence/P2.4-vertex-reflection.json`
  - 新增 `docs/evidence/P2.4-fragment-reflection.json`
- **验证**:
  - `make -C src/demos/d2 evidence`
  - 人工查看 `docs/evidence/P2.4-d2-textured-quad.png`，纹理 quad 正常可见
  - 检查 `docs/evidence/P2.4-fragment-reflection.json`，确认 `struct.top_level_global_ab` 为活跃绑定
- **关键发现**:
  - `metal-shaderconverter` 生成的片段 `metallib` 不直接消费 `setFragmentTexture(0)`，而是要求通过顶层参数缓冲传入纹理与采样器描述
  - D2 首次输出空白 PNG 的根因是 MSC 资源绑定约定未被满足，而不是截图或 PPM 转换逻辑异常

### 2026-06-12 | P2.3 D2 Textured Quad 接入 Path A 与纹理采样 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ `D2` 已通过 `Slang → DXIL → MSC → metallib` 渲染离屏纹理 quad
- **变更**:
  - 新增 `src/demos/d2/shaders/quad.slang`：顶点和片段入口，覆盖 quad 顶点与纹理采样
  - 新增 `src/demos/d2/src/main.cpp`：从 `metallib` 加载着色器，创建测试纹理并离屏渲染到 `textured_quad.ppm`
  - 新增 `src/demos/d2/Makefile`：构建 DXIL、metallib 和可执行文件
  - 更新 `src/demos/d2/README.md`：记录 D2 的构建、运行和产物
  - 更新 `src/demos/Makefile`：统一入口增加 `d2` 和 `run-d2`，`build-demos` 同时构建 D1、D2
  - 新增 `docs/evidence/P2.3-build.txt` 作为 Path A 构建与运行证据
- **验证**:
  - `make -C src/demos d2`
  - `make -C src/demos run-d2`
  - `file src/demos/d2/build/quad_vertex.metallib src/demos/d2/build/quad_fragment.metallib`
  - `file src/demos/d2/out/textured_quad.ppm`
  - `make build-demos`
- **说明**:
  - 当前 `slangc` 仍会对 `fragmentMain` 输出一条 `stage 'pixel' was specified more than once` 警告，但不阻塞 DXIL 与 metallib 生成；P2.4 再结合实际图像输出决定是否需要进一步消警

### 2026-06-12 | P2.2 D1 运行验证与 PNG 证据产物 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ D1 已补齐人眼可读的 PNG 证据和运行说明
- **变更**:
  - 更新 `src/demos/d1/Makefile`：新增 `evidence` 目标，自动生成 `PPM`、`PNG`、运行说明和元数据 JSON
  - 更新 `src/demos/d1/README.md`：补充 `make -C src/demos/d1 evidence` 用法
  - 新增 `docs/evidence/P2.2-d1-triangle.ppm`
  - 新增 `docs/evidence/P2.2-d1-triangle.png`
  - 新增 `docs/evidence/P2.2-run.txt`
  - 新增 `docs/evidence/P2.2-meta.json`
- **验证**:
  - `make -C src/demos/d1 evidence`
  - `file docs/evidence/P2.2-d1-triangle.ppm docs/evidence/P2.2-d1-triangle.png`
  - `wc -c docs/evidence/P2.2-d1-triangle.ppm docs/evidence/P2.2-d1-triangle.png`
  - 人工查看 `docs/evidence/P2.2-d1-triangle.png`，图像正常显示彩色三角形
- **说明**:
  - 当前仍是离屏渲染验证；窗口呈现建议放在后续 Demo 或单独切片中处理

### 2026-06-12 | P2.1 D1 Hello Triangle 离屏渲染落地 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ `D1` 已实现并可在 Apple M1 上离屏渲染彩色三角形
- **变更**:
  - 新增 `src/demos/d1/src/main.cpp`：使用 `metal-cpp`、手写 MSL、`RenderPipelineState` 和 `drawPrimitives` 渲染到离屏纹理
  - 新增 `src/demos/d1/Makefile`：支持 `build` / `run`，默认从 `~/GraphicsExperiments/third_party/metal-cpp` 读取 `metal-cpp`
  - 更新 `src/demos/d1/README.md`：说明离屏产物、构建方式和 `METAL_CPP_DIR` 覆盖方式
  - 更新 `src/demos/Makefile`：`build-demos` 与 `d1` / `run-d1` 统一接入 D1
  - 新增 `docs/evidence/P2.1-build.txt` 作为构建与运行证据
- **验证**:
  - `make -C src/demos d1`
  - `make -C src/demos run-d1`
  - `file src/demos/d1/out/triangle.ppm`
  - `wc -c src/demos/d1/out/triangle.ppm`
- **说明**:
  - 当前 D1 采用离屏渲染输出 `triangle.ppm`，先验证最小 Metal 管线；P2.2 再补正式运行证据和帧缓冲说明

### 2026-06-12 | P2.0 Demo 规格、构建入口与证据格式收口 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ P2.0 完成，Phase 2 进入 D1 实现前的可执行状态
- **变更**:
  - 修复顶层 `Makefile`，使 `make help`、`make build-demos`、`make clean` 指向当前 `src/` 目录结构
  - 重写 `src/demos/Makefile`，提供 `build-demos`、`d1`、`run-d1` 等稳定命名入口
  - 新增 `docs/p2-demo-evidence.md`，统一 D1-D8 的构建日志、运行证据、性能 JSON 规范
  - 更新 `README.md` 与 `src/demos/README.md`，同步目录结构、P2 证据规范和当前阶段约束
  - 新增 `docs/evidence/P2.0-build-entry.txt` 作为构建入口验证证据
- **验证**:
  - `make help`
  - `make build-demos`
  - `make -C src/demos d1`
  - `python3 tools/verify_progress.py`
- **说明**:
  - `P2.0` 只要求入口路径和证据口径稳定，不要求 D1 真实编译通过；D1 的实际构建和运行从 `P2.1`、`P2.2` 开始

### 2026-06-12 | P2 任务扩充与 Demo 验收标准收口 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 将 P2 从 6 个粗任务扩展为 D1-D8 渐进式验证链
- **变更**:
  - 更新 `PROGRESS.md`：P2 增加 `P2.0` 到 `P2.12`，覆盖 D1-D8 构建、运行、证据和 D8 性能验证
  - 更新 `src/demos/README.md`：补充 D1-D8 路线图、任务映射、证据要求和约束
  - 重新生成 `NEXT_TASK.md`：下一任务切换为 `P2.0`
  - 将 Phase 1 阶段状态标记为完成，避免阶段状态与 P1 全部完成事实冲突
- **验证**:
  - `python3 tools/gen_next_task.py`
  - `python3 tools/verify_progress.py` → 25/132 任务完成，验证通过
- **说明**:
  - Rust FFI 两项从 P2 移出；当前蓝图主线是 C++/C# 混合，P2 应聚焦 Metal Demo 能力验证

### 2026-06-12 | 仓库重建与 P1.9 补提交 | ✅ 完成

- **Agent**: MiMoCode
- **结果**: ✅ 重建独立仓库 + 恢复丢失文件 + 更新进度
- **变更**:
  - 在 `~/metal-backend/` 初始化独立 git 仓库（替代 `~/` 级别的父仓库）
  - 从父仓库旧提交 `84c1359` 恢复 `tools/bench_compile.sh`（24KB）和 `docs/evidence/bench-20260612-040600.json`
  - 更新 `PROGRESS.md`：P1.9 → ✅，完成度 17/220 (7.7%)
  - 重新生成 `NEXT_TASK.md`：下一任务 → P2.1
- **验证**:
  - `git status` 工作区干净
  - `git log --oneline` 包含全部 22 个提交
- **说明**:
  - 之前 force push 覆盖了远程历史，已从父仓库 `metal-backend/main` 分支恢复
  - 旧历史 20 个提交 + bench_compile.sh 补提交 + 进度更新 = 3 个新提交
- **归档**: `docs/session-logs/phase-1.md`

### 2026-06-12 凌晨 | P1.9 基准脚本交接规格准备 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 补齐 P1.9 文档输入，便于下一个会话直接实现基准脚本
- **变更**:
  - 新增 `docs/p1-bench-compile-spec.md`，定义 `must-pass / known-good / known-failure` 分桶、计时口径和输出字段
  - 新增 `docs/p4-shader-risk-checklist.md`，把 P1 语料发现映射到 P4/P5 的阻塞点和延后项
  - 更新 `docs/shader-corpus.md`，串起语料策略、P4 风险和 P1.9 规格
- **说明**:
  - 本次仅做交接文档准备，不变更 `PROGRESS.md` 任务状态
  - 下一会话应直接进入 `P1.9 bench_compile.sh` 实现

### 2026-06-12 凌晨 | P1.8b 扩展真实/近真实着色器语料回归测试 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ P1.8b 完成，`tools/test_shader_corpus_path_a.sh` 通过
- **变更**:
  - 新增 `tools/test_shader_corpus_path_a.sh`，覆盖确定性 VS/FS/CS 控制样本与本地 `deko3d_slang_poc/test_output` 语料扫描
  - 新增 `docs/shader-corpus.md`，记录正式语料、灰区来源和后续回归测试建议
  - 新增 `docs/evidence/P1.8b-shader-corpus-path-a.log` 作为任务证据
  - 更新 `docs/toolchain.md`，记录 `test_output` 语料规模和 P1.8b 发现
- **验证**:
  - `bash tools/test_shader_corpus_path_a.sh` → 29 Path A 通过 / 0 控制样本失败 / 13 语料发现问题 / 1 跳过或预期问题
- **关键发现**:
  - `real_*`、`kirby_*`、`large_*`、`final_*` 等近真实样本会暴露未初始化变量、TEXCOORD overlap、tessellation/domain 语义误判
  - RyuSAK 不进入正式证据；bnsh-decoder 只在有合法 `.bnsh` 输入时采用
- **归档**: `docs/session-logs/phase-1.md`

### 2026-06-12 凌晨 | P1.8 真实/近真实计算着色器 Path A 测试 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ P1.8 完成，`tools/test_real_cs_path_a.sh` 通过
- **变更**:
  - 新增 `tools/test_real_cs_path_a.sh`，覆盖 Ryujinx compute 缓存侦察、deko3d sinewave compute、Slang compute 特性和 GLSL compute 对照样本
  - 新增 `docs/evidence/P1.8-real-cs-path-a.log` 作为任务证据
  - 更新 `docs/toolchain.md` 和 `docs/shader-debug.md`，记录 compute 阶段固定使用 `cs_6_0`
- **验证**:
  - `bash tools/test_real_cs_path_a.sh` → 7 Path A 通过 / 0 失败 / 3 侦察或预期问题
- **关键发现**:
  - 当前 Ryujinx 本地缓存未发现真实 compute MSL / compute shader dump
  - deko3d raw sinewave compute 可通过 Path A，但有 `entry point parameter treated as uniform` 警告
  - Slang 原生 compute 的 RWBuffer、groupshared/barrier、atomic、ByteAddressBuffer、RWTexture2D 均通过 DXIL→MSC
  - GLSL compute std430 触发 E36107，应作为 Path C 对照语料
- **归档**: `docs/session-logs/phase-1.md`

### 2026-06-12 凌晨 | P1.7 真实片段着色器 Path A 测试 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ P1.7 完成，`tools/test_real_fs_path_a.sh` 通过
- **变更**:
  - 新增 `tools/test_real_fs_path_a.sh`，覆盖 deko3d GLSL 片段、Slang 原生片段、内嵌片段特性和 Ryujinx Fragment MSL 抽样
  - 新增 `docs/evidence/P1.7-real-fs-path-a.log` 作为任务证据
  - 更新 `docs/toolchain.md` 和 `docs/shader-debug.md`，记录片段阶段固定使用 `ps_6_0`
- **验证**:
  - `bash tools/test_real_fs_path_a.sh` → 11 Path A 通过 / 0 失败 / 6 MSL 跳过
- **关键发现**:
  - 部分简单片段样本使用 `sm_6_0` 时 slangc 返回 0 但不生成 DXIL
  - 片段阶段使用 `-stage fragment -profile ps_6_0` 可稳定生成 DXIL 并通过 MSC
- **归档**: `docs/session-logs/phase-1.md`

### 2026-06-12 凌晨 | 状态机循环加固 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 修复 PROGRESS.md / NEXT_TASK.md 统计漂移问题
- **变更**:
  - 增强 `tools/gen_next_task.py`：统一解析任务表、识别当前阶段、刷新 PROGRESS.md 顶部与底部统计、生成带状态的 NEXT_TASK.md
  - 增强 `tools/verify_progress.py`：检查顶部完成度、底部统计、NEXT_TASK.md 指向任务是否与 PROGRESS.md 一致
  - 调整 `.githooks/pre-commit`：先生成并暂存 PROGRESS.md + NEXT_TASK.md，再执行一致性验证
  - 刷新 PROGRESS.md 统计区：下一任务从过期的 P1.1 修正为 P1.7
- **验证**:
  - `python3 -m py_compile tools/gen_next_task.py tools/verify_progress.py`
  - `python3 tools/gen_next_task.py`
  - `python3 tools/verify_progress.py` → 21/124 任务完成，验证通过
- **归档**: `docs/session-logs/phase-1.md`

### 2026-06-12 凌晨 | 会话日志阶段切片 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 将线性 SESSION_LOG.md 改为索引 + 最近滚动记录
- **变更**:
  - Phase 0 历史迁移到 `docs/session-logs/phase-0.md`
  - Phase 1 历史迁移到 `docs/session-logs/phase-1.md`
  - 新增 `docs/session-logs/README.md` 说明归档规则
- **后续建议**:
  - 任务级证据继续写入 `PROGRESS.md` 的证据列
  - 跨任务技术结论沉淀到 `docs/*.md`，不要只留在会话日志中
