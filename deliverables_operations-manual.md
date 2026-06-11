# Switch Metal Backend — 操作手册

> 将项目模板上传到 GitHub，并用另一个 Agent 验证自动工作流。

---

## 第一步：上传模板到 GitHub


### 1.1 初始化 Git 并推送


# 初始化仓库
git init
git add -A
git commit -m "初始化：Switch Metal 后端项目模板 (Phase 0 完成, 15/220 任务)"

# 在 GitHub 上创建新仓库（假设叫 metal-backend）
# https://github.com/new — 不要勾选 README/.gitignore/LICENSE（模板已含）

# 关联远程并推送
git remote add origin git@github.com:skybuleli/metal-backend.git
git branch -M main
git push -u origin main
```

### 1.2 启用 Git Hooks

```bash
# 配置 pre-commit hook（提交前自动验证 + 生成下一任务）
git config core.hooksPath .githooks
chmod +x .githooks/pre-commit
```

---

## 第二步：另一个 Agent 的工作流

### 2.1 文件驱动状态机

整个项目由 4 个核心文件驱动，不需要任何外部框架：

```
┌──────────────┐     ┌──────────────┐
│  AGENTS.md   │────▶│  NEXT_TASK   │────▶ 执行任务
│  入口文件     │     │  当前指令     │
└──────────────┘     └──────────────┘
                             │
       ┌─────────────────────┘
       ▼
┌──────────────┐     ┌──────────────┐
│  PROGRESS.md │◀────│  gen_next_   │
│  状态机       │     │  task.py     │
└──────────────┘     └──────────────┘
```

### 2.2 Agent 会话协议（Agent 视角）

任何 Agent 打开仓库后，按以下顺序执行：

```
1. 读取 AGENTS.md    → 了解身份、语言约束、架构决策
2. 读取 NEXT_TASK.md → 知道当前要做什么（比如 P1.1）
3. 读取 blueprint/   → 了解技术规格和架构约束
4. 读取 PROGRESS.md  → 了解全局进度上下文
5. 标记 🔄          → 在 PROGRESS.md 中将任务标记为进行中
6. 执行任务          → 写代码、跑测试、验证
7. 标记 ✅          → 任务完成，标记完成时间 + 证据
8. 生成下一个任务    → python3 tools/gen_next_task.py
9. 记录会话          → 追加 SESSION_LOG.md
10. Git 提交         → 按规范格式 commit
```

### 2.3 P1.1 任务的预期执行

以 P1.1（编写 `test_slang_dxil.sh`）为例，Agent 应该：

1. **读取 NEXT_TASK.md** → 看到任务描述、验收标准、参考文件
2. **读取 `tools/tools-verify.sh`** → 学习脚本风格
3. **读取 `docs/toolchain.md`** → 了解 Path A 命令
4. **编写 `tools/test_slang_dxil.sh`**：
   - 内嵌 VS + PS 两个 GLSL 着色器
   - 调用 `slangc` 编译 GLSL → DXIL
   - 调用 `metal-shaderconverter` 转换 DXIL → metallib
   - 验证输出文件大小
   - 输出 PASS/FAIL
5. **运行验证**：`bash tools/test_slang_dxil.sh` → 期望看到 PASS
6. **更新 PROGRESS.md**：P1.1 标记 ✅
7. **运行 `python3 tools/gen_next_task.py`** → 自动生成 NEXT_TASK.md（P1.2）
8. **追加 SESSION_LOG.md**：记录本会话
9. **Git 提交**：`feat(tools): P1.1 Path A 端到端验证脚本 [done]`

---

## 第三步：验证流程是否正确运转

### 3.1 检查清单

在另一个 Agent 完成 P1.1 后，检查以下项目：

| 检查点 | 文件 | 预期结果 |
|--------|------|----------|
| PROGRESS.md | P1.1 行 | `✅ 完成时间 证据` |
| PROGRESS.md | P1.2 行 | `🔄` (进行中，仅当已开始) |
| NEXT_TASK.md | 任务 ID | 指向 P1.2 或下一个 `⬜` 任务 |
| SESSION_LOG.md | 末尾 | 新增一条会话记录 |
| tools/ | 新文件 | `test_slang_dxil.sh` 存在 |
| tools/ | 脚本可执行 | `bash tools/test_slang_dxil.sh` 输出 PASS |
| Git log | 最新 commit | `feat(tools): P1.1 Path A 端到端验证脚本 [done]` |

### 3.2 可能遇到的问题

| 问题 | 原因 | 解决 |
|------|------|------|
| Agent 用英文写注释 | 没读 AGENTS.md §1 语言约束 | 在 AGENTS.md 中加 `⚠️ 阅读此文件是强制要求` |
| Agent 跳过 blueprint/ | 没按会话协议执行 | 检查 AGENTS.md §3 是否清晰 |
| gen_next_task.py 报错 | Python 未安装或路径问题 | 确保 `python3` 可用 |
| pre-commit hook 不触发 | `core.hooksPath` 未配置 | 重新执行 `git config core.hooksPath .githooks` |
| Agent 修改了 ADR | Agent 不遵守 ADR 锁定规则 | AGENTS.md 中有明确声明，如违反应在 code review 拦截 |

### 3.3 推荐的新 Agent

以下 Agent 都能正确读取 `AGENTS.md` 并遵循协议：

- **Claude Code**（通过 `claude` CLI）
- **Cursor**（内置 Agent 模式）
- **GitHub Copilot**（Workspace / Chat 模式）
- **Windsurf**（Cascade 模式）

建议先用 **Claude Code** 测试，因为它对 `AGENTS.md` 的支持最成熟。

---

## 第四步：长期运转

### 4.1 日常节奏

```
Agent A 会话              Agent B 会话              Agent C 会话
    │                         │                         │
    ├─ P1.1 编写脚本          ├─ 读取 NEXT_TASK         ├─ 读取 NEXT_TASK
    ├─ 验证通过               ├─ P1.2 编写测试          ├─ P1.3 集成测试
    ├─ 更新 PROGRESS          ├─ 验证通过               ├─ ...
    ├─ git commit             ├─ 更新 PROGRESS          │
    └─ push ─────────────────▶├─ git commit             │
                              └─ push ─────────────────▶│
```

每次提交通过 CI 自动验证，pre-commit hook 自动生成下一个任务。

### 4.2 文件保护规则

以下文件**只能由人类修改**，Agent 绝不能改动：

- `AGENTS.md` — 项目宪法
- `blueprint/architecture.md` — ADR 锁定
- `blueprint/agent-ops.md` — 操作规范
- `README.md` / `CONTRIBUTING.md` — 项目门面

Agent 可以修改：
- `PROGRESS.md` — 标记任务状态
- `NEXT_TASK.md` — 通过脚本生成
- `SESSION_LOG.md` — 追加记录
- `src/` / `tools/` / `tests/` — 写代码

---

## 附录：文件大小参考

| 文件 | 大小 | 说明 |
|------|------|------|
| AGENTS.md | 7.3KB | Agent 入口约束 |
| PROGRESS.md | 14.4KB | 220 任务完整清单 |
| NEXT_TASK.md | 1.6KB | 当前任务 = P1.1 |
| blueprint/architecture.md | 3.1KB | Mermaid 架构图 + ADR |
| blueprint/agent-ops.md | 1.6KB | Agent 操作规范 |
| tools/tools-verify.sh | 3.6KB | 15 工具检查 |
| tools/gen_next_task.py | 3.7KB | 自动生成下一任务 |
| tools/verify_progress.py | 3.5KB | PROGRESS 格式验证 |

---

> **下一步**：在 GitHub 上创建仓库 → 推送模板 → 用 Claude Code 或 Cursor 打开仓库 → 看它是否能自动读取 AGENTS.md 并执行 P1.1。
