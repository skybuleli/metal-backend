# 会话日志

## 格式说明
每次会话追加一条记录。

---

## 2026-06-11 全天 | Phase 0 (P0.1–P0.12) | ✅ 完成

- **Agent**: Ally (AllyHub)
- **结果**: ✅ Phase 0 全部 15/15 任务完成
- **变更**:
  - 验证全部工具链（devkitPro、slangc、MSC 4.0、SPIRV-Tools 6 件套、glslangValidator、Rust、.NET）
  - 确认 MSC 在 CLT-only 环境下可用（与 Apple 文档不同，关键发现）
  - 确认 metal-cpp 头文件在 CLT SDK 中存在
  - 克隆 Ryubing (`~/dev/ryubing/`) + dotnet build 通过
  - 克隆 dxmt (`~/dev/dxmt/`, 3Shain/dxmt v0.80)
  - 创建完整仓库模板（AGENTS.md、PROGRESS.md、NEXT_TASK.md、SESSION_LOG.md）
  - 创建 4 个 Ally Skill：toolchain、shader-debug、metal-api、gal-mapping
  - 编写 tools-verify.sh（15 项检查 + Path A 端到端）
  - 配置 VS Code 工作区
- **关键发现**:
  - Path A（Slang→DXIL→MSC）完全可用
  - Path B/C 需完整 Xcode，暂非阻塞
  - slangc DXIL 输出需要 `-profile sm_6_0` 参数
  - dxmt 用 3Shain/dxmt（v0.80），非骨架版
- **问题**: Shader Playground 网站离线 → 本地验证替代
- **Commit**: 仓库模板初始化

---

## 2026-06-11 晚间 | 仓库模板完善 | ✅ 完成

- **Agent**: Ally (AllyHub)
- **结果**: ✅ 仓库模板从 29 文件补充至 45 文件
- **变更**:
  - 创建 tools/gen_next_task.py — 自动解析 PROGRESS.md 生成 NEXT_TASK.md
  - 创建 tools/verify_progress.py — 验证 PROGRESS.md 格式
  - 创建 .github/workflows/ci.yml — 4 Job CI/CD 流水线
  - 创建 .github/CODEOWNERS + CHANGELOG.md
  - 创建 5 个 src/ 骨架目录（libmetal_bridge/ryubing/demos/tests/ci）
  - 增强 blueprint/architecture.md（Mermaid 架构图）
  - 归档 9 个蓝图 HTML 至 blueprint/archive/
  - 完善 .gitignore
  - 更新 PROGRESS.md（15/220, 6.8%）+ NEXT_TASK.md（→P1.1）
- **问题**: 沙箱文件暂存缓存导致需直接覆写
- **Commit**: 仓库模板完善

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
  - 项点着色器 DXIL 3128 bytes → metallib 6116 bytes
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
