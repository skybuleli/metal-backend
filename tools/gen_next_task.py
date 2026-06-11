#!/usr/bin/env python3
"""从 PROGRESS.md 生成 NEXT_TASK.md。

解析 PROGRESS.md 找到第一个 ⬜ 状态的任务，提取其任务 ID 和描述，
生成 NEXT_TASK.md 文件供 Agent 读取。

用法：python3 tools/gen_next_task.py
"""

import argparse
import re
from datetime import datetime
from pathlib import Path


TASK_RE = re.compile(
    r"^\|\s*(P\d+\.\d+(?:\.\d+)?[a-z]?)\s*\|\s*(.*?)\s*\|\s*([⬜🔄✅🚫⏸️])\s*\|\s*(.*?)\s*\|\s*(.*?)\s*\|"
)
PHASE_RE = re.compile(r"^##\s*Phase\s+(\d+)\s*[:：—\-]\s*(.+?)\s*$")


def read_progress(progress_path: Path) -> str | None:
    """读取 PROGRESS.md。"""
    if not progress_path.exists():
        print(f"错误：找不到 {progress_path}")
        return None

    return progress_path.read_text(encoding="utf-8")


def parse_tasks(content: str) -> list[dict]:
    """解析任务表，保留任务所在阶段。"""
    tasks = []
    current_phase = "未知"

    for line in content.splitlines():
        phase_match = PHASE_RE.match(line)
        if phase_match:
            current_phase = f"Phase {phase_match.group(1)} — {phase_match.group(2).strip()}"
            continue

        task_match = TASK_RE.match(line)
        if task_match:
            tasks.append(
                {
                    "task_id": task_match.group(1),
                    "task_desc": task_match.group(2).strip(),
                    "status": task_match.group(3),
                    "done_at": task_match.group(4).strip(),
                    "evidence": task_match.group(5).strip(),
                    "phase": current_phase,
                }
            )

    return tasks


def find_next_task(tasks: list[dict]) -> dict | None:
    """找到当前进行中任务，或第一个待开始任务。"""
    for task in tasks:
        if task["status"] == "🔄":
            return task

    for task in tasks:
        if task["status"] == "⬜":
            return task

    return None


def find_current_phase(tasks: list[dict], task: dict | None) -> str:
    """找到当前活动阶段。"""
    if task is not None:
        return task["phase"]

    for task_item in tasks:
        if task_item["status"] in {"⬜", "🔄"}:
            return task_item["phase"]

    if tasks:
        return tasks[-1]["phase"]

    return "未知"


def task_counts(tasks: list[dict]) -> dict:
    """统计任务状态。"""
    total = len(tasks)
    completed = sum(1 for task in tasks if task["status"] == "✅")
    in_progress = sum(1 for task in tasks if task["status"] == "🔄")
    blocked = sum(1 for task in tasks if task["status"] == "🚫")
    pending = sum(1 for task in tasks if task["status"] == "⬜")
    skipped = sum(1 for task in tasks if task["status"] == "⏸️")
    percent = round(completed / total * 100, 1) if total else 0.0
    return {
        "total": total,
        "completed": completed,
        "in_progress": in_progress,
        "blocked": blocked,
        "pending": pending,
        "skipped": skipped,
        "percent": percent,
    }


def refresh_progress_stats(content: str, tasks: list[dict], next_task: dict | None, current_phase: str) -> str:
    """刷新 PROGRESS.md 顶部和底部统计，避免手写字段漂移。"""
    counts = task_counts(tasks)
    now = datetime.now().astimezone().isoformat(timespec="seconds")
    next_task_text = (
        f"{next_task['task_id']} — {next_task['task_desc']}"
        if next_task is not None
        else "无"
    )

    content = re.sub(
        r"^# 最后更新: .*$",
        f"# 最后更新: {now}",
        content,
        flags=re.MULTILINE,
    )
    content = re.sub(
        r"^# 当前阶段: .*$",
        f"# 当前阶段: {current_phase}",
        content,
        flags=re.MULTILINE,
    )
    content = re.sub(
        r"^# 完成度: .*$",
        f"# 完成度: {counts['completed']}/{counts['total']} 任务 ({counts['percent']}%)",
        content,
        flags=re.MULTILINE,
    )

    stats_block = (
        "# ===================================================================\n"
        "## ── 统计 ──\n"
        "# ===================================================================\n"
        f"- 总任务数: {counts['total']}\n"
        f"- 已完成: {counts['completed']} ({counts['percent']}%)\n"
        f"- 进行中: {counts['in_progress']}\n"
        f"- 阻塞: {counts['blocked']}\n"
        f"- 跳过: {counts['skipped']}\n"
        f"- 待开始: {counts['pending']}\n"
        f"- 当前阶段: {current_phase}\n"
        f"- 下一任务: {next_task_text}"
    )

    return re.sub(
        r"# ===================================================================\n## ── 统计 ──\n# ===================================================================\n(?:- .*(?:\n|$))+",
        stats_block + "\n",
        content,
        count=1,
    )


def generate_next_task(progress_path: Path, output_path: Path) -> bool:
    """刷新 PROGRESS.md 统计并生成 NEXT_TASK.md。"""
    content = read_progress(progress_path)
    if content is None:
        return False

    tasks = parse_tasks(content)
    task = find_next_task(tasks)
    phase = find_current_phase(tasks, task)

    refreshed_content = refresh_progress_stats(content, tasks, task, phase)
    if refreshed_content != content:
        progress_path.write_text(refreshed_content, encoding="utf-8")

    if task is None:
        print("所有任务已完成！")
        output_path.write_text(
            "# 所有任务已完成 🎉\n\n"
            "PROGRESS.md 中没有待开始的任务。\n\n"
            f"生成时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n",
            encoding="utf-8",
        )
        return True

    title = "当前任务" if task["status"] == "🔄" else "下一任务"

    content = f"""# {title} — {task['task_id']} {task['task_desc']}

> 生成时间：{datetime.now().strftime('%Y-%m-%d %H:%M:%S')}
> 所属阶段：{phase}

## 任务详情

| 项目 | 内容 |
|------|------|
| 任务 ID | {task['task_id']} |
| 任务名称 | {task['task_desc']} |
| 当前状态 | {task['status']} |
| 优先级 | 🔴 高 |
| 前置依赖 | 参见 PROGRESS.md |

## 执行步骤

1. 阅读相关技术规格（`blueprint/architecture.md`）
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
- [blueprint/architecture.md](blueprint/architecture.md) — 技术规格
"""
    output_path.write_text(content, encoding="utf-8")
    print(f"已生成 {output_path}")
    print(f"下一任务：{task['task_id']} — {task['task_desc']}")
    return True


def main():
    parser = argparse.ArgumentParser(description="从 PROGRESS.md 生成 NEXT_TASK.md")
    parser.add_argument("--path", default="PROGRESS.md", help="PROGRESS.md 路径")
    parser.add_argument("--output", default="NEXT_TASK.md", help="NEXT_TASK.md 输出路径")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parent.parent
    progress_path = Path(args.path)
    output_path = Path(args.output)
    if not progress_path.is_absolute():
        progress_path = repo_root / progress_path
    if not output_path.is_absolute():
        output_path = repo_root / output_path

    success = generate_next_task(progress_path, output_path)
    exit(0 if success else 1)


if __name__ == "__main__":
    main()
