# Phase 1 会话日志

## 阶段摘要

- **阶段**: Phase 1 — 着色器管道概念验证
- **当前状态**: 进行中
- **已完成任务**: P1.1–P1.8
- **下一任务**: P1.9 bench_compile.sh 基准测试脚本
- **关键发现**:
  - Path A 端到端可用，但 Path A 输入应转向 Slang 原生语法
  - GLSL std140 UBO / push_constant 在 slangc DXIL SM 6.0 下不兼容
  - `gl_PointSize` 在 DXIL SM 6.0 VS 中无 SV_PointSize 语义
  - 片段阶段 Path A 固定使用 `ps_6_0`，避免 slangc 返回 0 但无 DXIL
  - compute 阶段 Path A 固定使用 `cs_6_0`，Slang 原生 compute 特性可通过 MSC
  - Path B 可生成 MSL，但 CLT-only 环境无法用 `xcrun metal` 编译 metallib

---

## 2026-06-12 凌晨 | P1.1 Path A 端到端验证脚本 | ✅ 完成

- **Agent**: Qoder
- **结果**: ✅ P1.1 完成，test_slang_dxil.sh 4/4 PASS
- **变更**:
  - 编写 tools/test_slang_dxil.sh：内嵌 VS + FS 两个 GLSL 着色器，验证 GLSL→DXIL→metallib 全流程
  - 修复 verify_progress.py + gen_next_task.py 正则，支持三级任务 ID（P4.1.1）
  - 补充 .githooks/pre-commit 调用 verify_progress.py
  - 校正 PROGRESS.md 统计：76→124 个任务
- **关键发现**:
  - GLSL 460 UBO 语法（`uniform Uniforms { mat4 uMVP; }`）与 slangc DXIL SM 6.0 不兼容，需用简化写法
  - 顶点着色器 DXIL 3128 bytes → metallib 6116 bytes
  - 片段着色器 DXIL 3828 bytes → metallib 5388 bytes
- **Commit**: `feat(tools): P1.1 Path A 端到端验证脚本 [done]`

---

## 2026-06-12 凌晨 | P1.2 Path C SPIR-V 验证脚本 | ✅ 完成

- **Agent**: Qoder
- **结果**: ✅ P1.2 完成，test_glslang_spirv.sh 8/8 PASS
- **变更**:
  - 编写 tools/test_glslang_spirv.sh：GLSL→SPIR-V + spirv-val + spirv-opt 全流程
  - 顶点着色器 SPIR-V 1016 bytes，片段着色器 1144→888 bytes（优化后）
- **关键发现**:
  - glslangValidator 不识别 `.glsl` 扩展名，需用 `.vert.glsl` / `.frag.glsl` 复合后缀
  - spirv-opt -O 对简单着色器优化效果有限（顶点不变，片段 1144→888）
- **Commit**: `feat(tools): P1.2 Path C SPIR-V 验证脚本 [done]`

---

## 2026-06-12 凌晨 | P1.3 Slang→SPIR-V 验证脚本 | ✅ 完成

- **Agent**: Qoder
- **结果**: ✅ P1.3 完成，test_slang_spirv.sh 8/8 PASS
- **变更**:
  - 编写 tools/test_slang_spirv.sh：Slang→SPIR-V + spirv-val + spirv-dis + 交叉对比
  - 顶点着色器 SPIR-V 584 bytes（41 行反汇编）
  - 片段着色器 SPIR-V 836 bytes（53 行反汇编）
- **关键发现**:
  - Slang 生成的 SPIR-V 比 glslangValidator 小 43%（584 vs 1016 bytes），优化更激进
  - 两种编译器输出均通过 spirv-val 验证
- **Commit**: `feat(tools): P1.3 Slang→SPIR-V 验证脚本 [done]`

---

## 2026-06-12 凌晨 | P1.4 Path C 端到端验证脚本 | ✅ 完成

- **Agent**: Qoder
- **结果**: ✅ P1.4 完成，test_spirv_cross_msl.sh 10/10 PASS
- **变更**:
  - 编写 tools/test_spirv_cross_msl.sh：GLSL→SPIR-V→opt→MSL 全流程
  - 顶点着色器：1016 bytes SPIR-V → 447 bytes MSL (25 行)
  - 片段着色器：1144→888 bytes SPIR-V → 617 bytes MSL (24 行)
  - MSL 输出包含 metal_stdlib + vertex/fragment 关键字
- **关键发现**:
  - spirv-cross MSL 输出紧凑有效，包含完整 Metal 着色器结构
  - MSL→metallib 需完整 Xcode (xcrun metal)，当前 CLT 环境暂不可用，非阻塞
- **Commit**: `feat(tools): P1.4 Path C 端到端验证脚本 [done]`

---

## 2026-06-12 凌晨 | P1.5 Path B 尝试脚本 | ✅ 完成

- **Agent**: Qoder
- **结果**: ✅ P1.5 完成，test_slang_metal.sh 4 PASS + 2 预期 SKIP
- **变更**:
  - 编写 tools/test_slang_metal.sh：Slang→MSL + 环境检测 + xcrun metal 尝试
  - 顶点着色器 MSL 1060 bytes (64 行)，片段着色器 MSL 735 bytes (49 行)
  - xcrun metal 不可用（CLT-only），整体仍 PASS
- **关键发现**:
  - Slang `-target metal` 的 fragment 阶段不支持 GLSL `texture()` 采样，需简化着色器
  - Slang MSL 输出有效但无法编译为 metallib（需完整 Xcode）
  - Path B 部分可用：MSL 生成 OK，编译 metallib 不可用
- **Commit**: `feat(tools): P1.5 Path B 尝试脚本 [done]`

---

## 2026-06-12 凌晨 | P1.6 真实顶点着色器 Path A 测试 | ✅ 完成

- **Agent**: Qoder
- **结果**: ✅ P1.6 完成，test_real_vs_path_a.sh 3 PASS + 2 预期失败 + 5 MSL 跳过
- **数据源**:
  - deko3d_metal_runtime/shaders/triangle_vs.glsl → Path A PASS (269→3176→6216 bytes)
  - deko3d_slang_poc/shaders/triangle.vert.glsl → Path A PASS (321→3176→6216 bytes)
  - deko3d_slang_poc/output/roundtrip_vert.glsl → Path A PASS (319→3176→6216 bytes)
  - complex.vert.glsl (UBO) → 预期失败（GLSL std140 UBO 与 slangc DXIL SM 6.0 不兼容）
  - complex_v4.vert.glsl (push_constant) → 预期失败（push_constant 是 Vulkan 特有语法）
  - Ryujinx msl_dump 5 个 MSL 抽样 → 有效但需 xcrun metal
- **关键发现**:
  - 简单 GLSL 着色器 Path A 完全可用
  - GLSL UBO/push_constant 在 slangc DXIL 下不兼容，需 Slang 原生语法
  - Ryujinx msl_dump 是完整游戏着色器 (600~1000 行)，MSL 格式有效
- **Commit**: `feat(tools): P1.6 真实顶点着色器 Path A 测试 [done]`

---

## P1.6 补充 — 多样化特性覆盖（2026-06-12）

- **任务**: 补充 P1.6 着色器验证覆盖不足问题
- **结果**: 11 PASS / 0 FAIL / 8 预期跳过（原 3 PASS / 0 FAIL / 7 SKIP）
- **新增测试**:
  - 矩阵运算 (mat4*mat4, mat4*vec4) ✅
  - 条件分支 (if/else + ternary) ✅
  - 循环 (for + while) ✅
  - 数学函数 (sin/cos/pow/sqrt/abs/clamp/mix/min/max) ✅
  - 多输出 (多个 varying) ✅ (无 gl_PointSize) / ❌ (含 gl_PointSize → 已知失败)
  - 整数位运算 (&/|/<</>>) ✅
  - 数组 + swizzle ✅
  - SPV→GLSL roundtrip (vertex_0003.spv → spirv-cross → Path A) ✅
- **新发现不兼容**:
  - `gl_PointSize` 在 DXIL SM 6.0 VS 中无 SV_PointSize 语义
  - Switch 游戏常用点精灵，P4/P8 需处理
- **文档同步**: 兼容性矩阵从 12 → 22 项特性，失败模式表从 3 → 4 条

---

## 2026-06-12 凌晨 | 状态机循环加固 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 修复 PROGRESS.md / NEXT_TASK.md 统计漂移问题
- **变更**:
  - 增强 tools/gen_next_task.py：统一解析任务表、识别当前阶段、刷新 PROGRESS.md 顶部与底部统计、生成带状态的 NEXT_TASK.md
  - 增强 tools/verify_progress.py：检查顶部完成度、底部统计、NEXT_TASK.md 指向任务是否与 PROGRESS.md 一致
  - 调整 .githooks/pre-commit：先生成并暂存 PROGRESS.md + NEXT_TASK.md，再执行一致性验证
  - 刷新 PROGRESS.md 统计区：下一任务从过期的 P1.1 修正为 P1.7
- **验证**:
  - `python3 -m py_compile tools/gen_next_task.py tools/verify_progress.py`
  - `python3 tools/gen_next_task.py`
  - `python3 tools/verify_progress.py` → 21/124 任务完成，验证通过
- **后续建议**:
  - P1.7 开始前仍按 AGENTS.md 协议先标记 🔄
  - 后续可继续把证据文件路径结构化，但本次未强制改旧证据格式

---

## 2026-06-12 凌晨 | P1.7 真实片段着色器 Path A 测试 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ P1.7 完成，`tools/test_real_fs_path_a.sh` 通过
- **变更**:
  - 新增 `tools/test_real_fs_path_a.sh`：覆盖 deko3d GLSL 片段样本、Slang 原生真实片段样本、内嵌片段特性、Ryujinx Fragment MSL 抽样
  - 新增 `docs/evidence/P1.7-real-fs-path-a.log`：保存完整验证输出
  - 更新 `docs/toolchain.md`：记录片段阶段 Path A 命令使用 `ps_6_0`
  - 更新 `docs/shader-debug.md`：补充 FS profile 兼容性矩阵、失败模式和大小健康检查
- **验证**:
  - `bash tools/test_real_fs_path_a.sh` → 11 Path A 通过 / 0 失败 / 6 MSL 跳过 / 总计 17
  - `python3 tools/verify_progress.py` → 22/124 任务完成，验证通过
- **关键发现**:
  - 部分简单片段样本使用 `sm_6_0` 时 slangc 返回 0 但不生成 DXIL
  - 片段阶段固定 `-stage fragment -profile ps_6_0` 后，GLSL 与 Slang 原生片段样本均可生成 DXIL 并通过 MSC
  - Ryujinx Fragment MSL 抽样含 `metal_stdlib` 与 `fragment` 关键字；CLT-only 环境下仍仅做格式抽检
- **Commit**: `feat(tools): P1.7 真实片段着色器 Path A 测试 [done]`

---

## 2026-06-12 凌晨 | P1.8 真实/近真实计算着色器 Path A 测试 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ P1.8 完成，`tools/test_real_cs_path_a.sh` 通过
- **变更**:
  - 新增 `tools/test_real_cs_path_a.sh`：先侦察 Ryujinx 本地 compute 缓存，再验证 deko3d sinewave compute 和内嵌 Slang compute 特性
  - 新增 `docs/evidence/P1.8-real-cs-path-a.log`：保存完整验证输出
  - 更新 `docs/toolchain.md`：记录 compute 阶段 Path A 命令使用 `cs_6_0`
  - 更新 `docs/shader-debug.md`：补充 compute 特性矩阵、失败模式和大小健康检查
- **验证**:
  - `bash tools/test_real_cs_path_a.sh` → 7 Path A 通过 / 0 失败 / 3 侦察或预期问题 / 总计 10
  - `python3 tools/verify_progress.py` → 23/124 任务完成，验证通过
- **关键发现**:
  - 当前 Ryujinx 本地缓存未发现真实 compute MSL / compute shader dump，不能声称已有真实游戏 compute 覆盖
  - deko3d raw sinewave compute 可通过 Path A：4156B DXIL → 5360B metallib，但 slangc 提示无 system-value semantic 的参数被当作 uniform
  - 规整 Slang compute 覆盖 RWStructuredBuffer、groupshared、barrier、atomic、RWByteAddressBuffer、RWTexture2D，均可通过 DXIL→MSC
  - GLSL compute std430 触发 E36107，应作为 Path C 对照语料，不作为 Path A 主输入
- **Commit**: `feat(tools): P1.8 真实计算着色器 Path A 测试 [done]`
