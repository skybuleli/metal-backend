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
- 当前进度：25/132 任务完成
- 下一任务：P2.0 — 收口 P2 规格：同步 D1-D8 验收标准、构建入口和证据格式
- 最近状态机维护：`gen_next_task.py` 会刷新 `PROGRESS.md` 统计区并生成 `NEXT_TASK.md`；`verify_progress.py` 会校验二者一致性

## 最近滚动记录

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
