# 下一任务 — P1.1 编写 test_slang_dxil.sh

> 生成时间：2026-06-11 20:36 CST  
> 所属阶段：Phase 1 — 着色器管道概念验证  
> 当前进度：15/220 (6.8%) — Phase 0 ✅ 完成

## 任务详情

| 项目 | 内容 |
|------|------|
| 任务 ID | P1.1 |
| 任务名称 | 编写 test_slang_dxil.sh — Path A 端到端验证脚本 |
| 优先级 | 🔴 高（Phase 1 入口任务） |
| 预计耗时 | 30 分钟 |
| 前置依赖 | P0.2 (slangc), P0.11 (tools-verify.sh) |

## 任务描述

编写自动化测试脚本，验证 Path A 着色器编译管道的端到端可用性。

### 脚本要求

1. **输入**：内嵌 GLSL 测试着色器（VS + PS）
2. **步骤**：
   - `slangc` 编译 GLSL → DXIL（必须加 `-profile sm_6_0`）
   - `metal-shaderconverter` 转换 DXIL → metallib
   - 验证每步输出文件大小 > 0
   - 验证 metallib > DXIL（健康检查）
3. **输出**：明确的 PASS/FAIL 状态
4. **风格**：与 `tools/tools-verify.sh` 一致

## 验收标准

- [ ] 脚本可执行
- [ ] DXIL 文件生成且大小 > 0
- [ ] metallib 生成且大小 > DXIL
- [ ] VS + PS 均通过
- [ ] 清晰的 PASS/FAIL 输出

## 参考

- [tools/tools-verify.sh](tools/tools-verify.sh) — 参考风格
- [docs/toolchain.md](docs/toolchain.md) — Path A 命令速查
- [docs/shader-debug.md](docs/shader-debug.md) — 常见错误排查

## 完成后

1. PROGRESS.md 中标记 P1.1 为 ✅
2. 运行 `python3 tools/gen_next_task.py` 生成下一任务
3. 追加 SESSION_LOG.md
4. 提交：`feat(tools): P1.1 编写 Path A 端到端验证脚本 [done]`
