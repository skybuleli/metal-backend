# AGENTS.md — Switch Metal 后端

> **Agent 入口文件**。Cursor、Claude Code、Copilot、Windsurf、Codex 启动时自动读取。
> 最后更新：2026-06-14

---

## ⚡ 快速启动（每次会话必读，其余按需）

```bash
cat NEXT_TASK.md          # 第 1 步：找到当前任务
cat PROGRESS.md | head -5 # 第 2 步：确认整体进度
# 第 3 步：在 PROGRESS.md 中将任务 ⬜ → 🔄，然后开始干活
```

**完成后：**
```bash
python3 tools/verify_progress.py
git add -A && git commit -m "feat(metal): P{X}.{Y} 任务描述 [done]"
python3 tools/gen_next_task.py
# 追加会话记录到 SESSION_LOG.md
```

---

## 1. 项目身份

**是什么**：为 Ryubing（Nintendo Switch 模拟器）实现原生 Apple Metal 图形后端，绕过 Vulkan→MoltenVK 中间层。
**成功定义**：Top 20 Switch 游戏 ≥15 款可玩且 ≥30fps，2D 游戏 ≥60fps，运行于 Apple Silicon Mac。
**不是**：不替换 Ryubing、不支持非 Apple 平台、不是新模拟器。

### 语言约束（顶级强制）

**必须中文**：代码注释、Git 提交信息、文档、SESSION_LOG.md、PROGRESS.md 任务描述。
**可以英文**：代码标识符、技术术语（DXIL、SPIR-V、metallib）、命令行参数。

---

## 2. 四文件驱动系统

| 文件 | 角色 | 谁写 |
|------|------|------|
| `AGENTS.md` | 入口约束（本文件） | 人类 |
| `PROGRESS.md` | 220 任务状态机 ⬜/🔄/✅/🚫/⏸️ | Agent |
| `NEXT_TASK.md` | 当前指令（自动生成） | 脚本 |
| `SESSION_LOG.md` | 会话审计记录 | Agent |

**规则**：Agent 永远不需要猜测该干什么。读 `NEXT_TASK.md`，它已经告诉你一切。

---

## 3. 架构决策（ADR，禁止推翻）

| ADR | 决策 |
|-----|------|
| ADR-001 | **主着色器路径**：Slang→DXIL（`-profile sm_6_0`）→ MSC → metallib |
| ADR-002 | **模拟器基础**：Ryubing（有 GAL 抽象层 + IRenderer/IPipeline 接口） |
| ADR-003 | **混合架构**：C# 实现 GAL 接口 → P/Invoke → C++ libmetal_bridge（metal-cpp） |
| ADR-004 | **5 条着色器路径冗余**：A（主）→ C → B → 报错；详见 `blueprint/architecture.md` |

> 如需修改 ADR → 将任务标记 🚫 + 在阻塞表说明理由，等待人类批准。

---

## 4. 当前阶段速查（Phase 4）

**当前进度**：见 `PROGRESS.md` 第 1 行（动态，不写死）。
**Phase 4 目标**：2D 游戏可启动渲染。

### libmetal_bridge 模块状态

| 模块 | 状态 | 说明 |
|------|------|------|
| MetalDevice | ✅ P4.1 完成 | MTLDevice 创建、GPU 选择 |
| MetalBuffer | ✅ P4.1 完成 | UMA→Shared，Discrete→Managed/Private |
| MetalTexture | ✅ P4.1 完成 | 53 种像素格式映射 |
| MetalSampler | ✅ P4.1 完成 | MTLSamplerState |
| ShaderCompiler | 🔄 P4.2 进行中 | Slang API + libmetalirconverter + 缓存 |
| CommandMapper | ⬜ P4.3 待开始 | Maxwell 状态 → Metal API |
| Presenter | ⬜ P4.5 待开始 | CAMetalLayer、交换链 |

### 关键编码约束

- `NS_PRIVATE_IMPLEMENTATION` / `MTL_PRIVATE_IMPLEMENTATION` 仅在 `MetalDevice.cpp` 定义
- metal-cpp 调用必须在 `NS::AutoreleasePool` 作用域内
- MTL4Compiler：每设备仅一个实例；langVersion=3.2（非 4.0，M1/M2 超时）
- Buffer 绑定布局：`buffer(0)`=根表，`buffer(1)`=采样器，`buffer(2)`=per-draw

---

## 5. 编程规范

### 语言标准

| 语言 | 标准 | 关键规则 |
|------|------|----------|
| C++ | C++17 | metal-cpp，不用 ObjC Metal 头文件 |
| C# | C# 12 / .NET 8 | 遵循 Ryubing 风格；P/Invoke 桥接 |
| MSL | Metal 3.0 | 目标 Apple GPU Family 7+ |
| Python | 3.11+ | 带类型标注；仅标准库 |
| Shell | bash 3.2+ | `set -euo pipefail` |

### 全局规则（严禁违反）

1. **禁止臆测编码**：只实现当前任务要求的内容，禁止"顺便做"。
2. **禁止凭空发明 API**：Metal API 必须可追溯到 Apple 官方文档或 metal-cpp 头文件；Ryubing 接口必须读取 `src/Ryujinx.Graphics.GAL/` 源码确认。
3. **禁止跳过验证**：每个任务有出口标准，必须产出实际证据（编译输出、测试结果）。
4. **禁止无证据修改 PROGRESS.md**：每个 ✅ 必须含时间戳 + 证据文件路径。
5. **禁止同时执行两个任务**：PROGRESS.md 中只能有一个 🔄。
6. **一个任务一次提交**：禁止合并多个任务。
7. **禁止未读规格就写代码**：执行前必须读 `NEXT_TASK.md` + `blueprint/architecture.md` 相关章节。

### 事实锚定

代码中的每个事实性断言必须锚定到：`blueprint/architecture.md` / Apple Metal 官方文档 / 真实 Ryubing 源码（注明文件+行号）/ `workfiles/` 中已验证的实验输出。
无法锚定 → 写 `// TODO: 待验证`，不要猜。

---

## 6. 提交约定

```
<type>(<scope>): P{X}.{Y} <中文描述> [done|wip]

type:  feat | fix | docs | test | refactor | perf | build | ci | chore
scope: P0-P1→tools | P2→demo | P3→gal | P4→metal | P5→cmd | P6→test | P7→perf | P8→fix | P9→docs
```

---

## 7. 应急处理

| 情况 | 处理 |
|------|------|
| 任务被阻塞 | 标记 🚫 → 阻塞表填原因 → 找可解除阻塞的任务 → 无则停止并报告 |
| 构建失败 | 禁止继续下一任务，先修复，记录到 SESSION_LOG.md |
| 与蓝图冲突 | 蓝图优先；如蓝图有误 → 标记 🚫 + 说明，禁止自行"修复"蓝图 |

---

## 8. 按需读取索引

> 以下文档**不要在会话开始时全部读取**，遇到对应问题时再读。

| 遇到什么问题 | 读这个 |
|-------------|--------|
| 工具命令、路径、版本、已知陷阱 | `docs/toolchain.md` |
| 着色器编译错误、SPIR-V 验证 | `docs/shader-debug.md` |
| GAL 接口方法签名、Metal 映射 | `docs/gal-mapping.md` |
| metal-cpp API 模式、Device/Buffer/Texture | `docs/metal-api.md` |
| 完整架构 ADR、着色器路径细节 | `blueprint/architecture.md` |
| Agent 操作系统设计 | `blueprint/agent-ops.md` |
| Maxwell 寄存器参考 | deko3d `engine_3d.def` + envytools `rnndb` |
| Metal API 官方参考 | https://developer.apple.com/metal/ |
| MSC 用法与 CLI 参数 | https://developer.apple.com/metal/shader-converter/ |

---

*修改本文件时请更新顶部日期。*
