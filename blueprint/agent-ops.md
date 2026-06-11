# Agent 操作手册

## 四文件状态机

| 文件 | 角色 | 谁写 | 频率 |
|------|------|------|------|
| `PROGRESS.md` | 状态机 | Agent | 每个任务 |
| `NEXT_TASK.md` | 当前指令 | 脚本 | 每次提交 |
| `SESSION_LOG.md` | 历史记录 | Agent | 每次会话 |
| `blueprint/` | 技术规格 | 人类 | 方案变更时 |

## 会话生命周期

```
开始 → 读 NEXT_TASK.md
     → 在 PROGRESS.md 中标记 🔄
     → 按 blueprint/ 规格执行
     → 验证出口标准
     → 在 PROGRESS.md 中标记 ✅
     → git commit（约定式提交）
     → python3 tools/gen_next_task.py
     → 追加到 SESSION_LOG.md
结束
```

## PROGRESS.md 状态图标

`⬜` 待开始 · `🔄` 进行中 · `✅` 已完成 · `🚫` 已阻塞 · `⏸️` 已跳过

## 提交格式

```
<type>(<scope>): P{X}.{Y} <任务描述> [done]

type:  feat|fix|docs|test|refactor|perf|build|ci|chore
scope: tools|demo|gal|metal|shader|cmd|test|docs
```

## 各阶段的 Scope 对照

| 阶段 | Scope |
|------|-------|
| P0-P1 | tools |
| P2 | demo |
| P3 | gal |
| P4 | metal |
| P5 | cmd |
| P6 | test |
| P7 | perf |
| P8 | fix |
| P9 | docs |

## 阻塞处理流程

1. 将任务标记为 🚫
2. 在阻塞表中添加：阻塞ID / 描述 / 受影响任务 / 解决方案
3. 如可由其他任务解除 → 转去执行该任务
4. 如需人类介入 → 报告并等待

## 工具脚本

- `tools/gen_next_task.py` — 生成 NEXT_TASK.md
- `tools/verify_progress.py` — 验证 PROGRESS.md
- `.githooks/pre-commit` — 提交时自动验证 + 自动生成
