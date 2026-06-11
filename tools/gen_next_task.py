#!/usr/bin/env python3
"""从 PROGRESS.md 生成 NEXT_TASK.md。

解析 PROGRESS.md 找到第一个 ⬜ 状态的任务，提取其任务 ID 和描述，
生成 NEXT_TASK.md 文件供 Agent 读取。

用法：python3 tools/gen_next_task.py
"""

import re
from datetime import datetime
from pathlib import Path


def find_next_task(progress_path: Path) -> dict | None:
    """在 PROGRESS.md 中找到第一个未完成的任务。"""
    if not progress_path.exists():
        print(f"错误：找不到 {progress_path}")
        return None

    content = progress_path.read_text(encoding="utf-8")

    # 匹配格式：| P0.1 | 验证 devkitPro + CLT | ⬜ |
    # 或：- [ ] P0.1 验证 devkitPro
    patterns = [
        # 表格格式
        re.compile(r"\|\s*(P\d+\.\d+(?:\.\d+)?[a-z]?)\s*\|\s*(.+?)\s*\|\s*⬜\s*\|"),
        # 列表格式
        re.compile(r"-\s*\[\s*\]\s*(P\d+\.\d+(?:\.\d+)?[a-z]?)\s+(.+)"),
    ]

    for pattern in patterns:
        for match in pattern.finditer(content):
            task_id = match.group(1)
            task_desc = match.group(2).strip()
            return {
                "task_id": task_id,
                "task_desc": task_desc,
            }

    return None


def find_current_phase(progress_path: Path) -> str:
    """找到当前活动阶段。"""
    if not progress_path.exists():
        return "未知"

    content = progress_path.read_text(encoding="utf-8")
    # 匹配：## Phase N — 名称 ⬜ 待开始
    match = re.search(r"##\s*Phase\s+(\d+)\s*[—\-]\s*(.+?)\s*(?:⬜|🔄)", content)
    if match:
        return f"Phase {match.group(1)} — {match.group(2).strip()}"

    return "未知"


def generate_next_task(progress_path: Path, output_path: Path) -> bool:
    """生成 NEXT_TASK.md。"""
    task = find_next_task(progress_path)
    phase = find_current_phase(progress_path)

    if task is None:
        print("所有任务已完成！")
        output_path.write_text(
            "# 所有任务已完成 🎉\n\n"
            "PROGRESS.md 中没有待开始的任务。\n\n"
            f"生成时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n",
            encoding="utf-8",
        )
        return True

    content = f"""# 下一任务 — {task['task_id']} {task['task_desc']}

> 生成时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}  
> 所属阶段：{phase}

## 任务详情

| 项目 | 内容 |
|------|------|
| 任务 ID | {task['task_id']} |
| 任务名称 | {task['task_desc']} |
| 优先级 | 🔴 高 |
| 前置依赖 | 参见 PROGRESS.md |

## 执行步骤

1. 阅读相关技术规格（`.blueprint/architecture.md`）
2. 在 PROGRESS.md 中将任务标记为 🔄
3. 执行任务
4. 验证出口标准
5. 在 PROGRESS.md 中将任务标记为 ✅
6. 提交代码（约定式提交格式）
7. 运行本脚本重新生成此文件

## 完成后

- 更新 PROGRESS.md：{task['task_id']} → ✅
- 重新运行 `python3 tools/gen_next_task.py` 生成下一个任务
- 记录 SESSION_LOG.md

## 参考

- [AGENTS.md](AGENTS.md) — Agent 行为规范
- [PROGRESS.md](PROGRESS.md) — 总体进度
- [.blueprint/architecture.md](.blueprint/architecture.md) — 技术规格
"""
    output_path.write_text(content, encoding="utf-8")
    print(f"已生成 {output_path}")
    print(f"下一任务：{task['task_id']} — {task['task_desc']}")
    return True


def main():
    repo_root = Path(__file__).resolve().parent.parent
    progress_path = repo_root / "PROGRESS.md"
    output_path = repo_root / "NEXT_TASK.md"

    success = generate_next_task(progress_path, output_path)
    exit(0 if success else 1)


if __name__ == "__main__":
    main()
