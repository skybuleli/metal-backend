#!/usr/bin/env python3
"""验证 PROGRESS.md 的格式与一致性。

检查项：
1. 任务总数与完成数的一致性
2. 不能有两个 🔄 任务同时存在
3. 任务 ID 不能重复
4. ✅ 任务必须有完成日期
5. 百分比计算正确

用法：python3 tools/verify_progress.py
"""

import re
import sys
from pathlib import Path

from gen_next_task import find_current_phase, find_next_task, parse_tasks, task_counts


def verify_progress(progress_path: Path, next_task_path: Path) -> tuple[bool, list[str]]:
    """验证 PROGRESS.md。返回 (是否通过, 错误列表)。"""
    errors = []

    if not progress_path.exists():
        errors.append(f"文件不存在：{progress_path}")
        return False, errors

    content = progress_path.read_text(encoding="utf-8")
    tasks = parse_tasks(content)
    counts = task_counts(tasks)
    next_task = find_next_task(tasks)
    current_phase = find_current_phase(tasks, next_task)

    # 检查 1：不能有两个 🔄 同时存在
    if counts["in_progress"] > 1:
        errors.append(f"发现 {counts['in_progress']} 个 🔄（进行中）任务，同时只能有一个！")

    # 检查 2：任务总数与进度百分比
    if counts["total"] == 0:
        errors.append("未找到任何任务行（格式：| P0.1 | 描述 | 状态 |）")
    else:
        header_match = re.search(
            r"^# 完成度:\s*(\d+)/(\d+)\s*任务\s*\((\d+\.?\d*)%\)",
            content,
            flags=re.MULTILINE,
        )
        if header_match:
            reported_completed = int(header_match.group(1))
            reported_total = int(header_match.group(2))
            reported_pct = float(header_match.group(3))
            if reported_completed != counts["completed"] or reported_total != counts["total"]:
                errors.append(
                    "顶部完成度不一致："
                    f"报告中 {reported_completed}/{reported_total}，"
                    f"实际 {counts['completed']}/{counts['total']}"
                )
            if abs(reported_pct - counts["percent"]) > 0.1:
                errors.append(
                    f"顶部百分比不一致：报告中 {reported_pct}%，实际 {counts['percent']}%"
                )
        else:
            errors.append("缺少顶部完成度行：# 完成度: N/M 任务 (P%)")

    # 检查 3：重复的任务 ID
    task_ids = [task["task_id"] for task in tasks]
    seen = {}
    for tid in task_ids:
        if tid in seen:
            errors.append(f"重复的任务 ID：{tid}")
        seen[tid] = True

    # 检查 4：✅ 任务必须有日期
    for task in tasks:
        if task["status"] == "✅" and not re.search(r"\d{4}-\d{2}-\d{2}", task["done_at"]):
            errors.append(f"{task['task_id']} 标记为 ✅ 但没有完成日期")

    # 检查 5：底部统计必须与任务表一致
    expected_stats = {
        "总任务数": str(counts["total"]),
        "已完成": f"{counts['completed']} ({counts['percent']}%)",
        "进行中": str(counts["in_progress"]),
        "阻塞": str(counts["blocked"]),
        "跳过": str(counts["skipped"]),
        "待开始": str(counts["pending"]),
        "当前阶段": current_phase,
        "下一任务": (
            f"{next_task['task_id']} — {next_task['task_desc']}"
            if next_task is not None
            else "无"
        ),
    }
    for key, expected in expected_stats.items():
        stat_match = re.search(rf"^- {re.escape(key)}:\s*(.+)$", content, flags=re.MULTILINE)
        if not stat_match:
            errors.append(f"统计区缺少字段：{key}")
            continue
        actual = stat_match.group(1).strip()
        if actual != expected:
            errors.append(f"统计区字段 {key} 不一致：报告中 {actual}，实际 {expected}")

    # 检查 6：NEXT_TASK.md 必须与 PROGRESS.md 的当前/下一任务一致
    if not next_task_path.exists():
        errors.append(f"文件不存在：{next_task_path}")
    elif next_task is not None:
        next_task_content = next_task_path.read_text(encoding="utf-8")
        id_match = re.search(r"\| 任务 ID \|\s*(P\d+\.\d+(?:\.\d+)?[a-z]?)\s*\|", next_task_content)
        if not id_match:
            errors.append("NEXT_TASK.md 缺少任务 ID")
        elif id_match.group(1) != next_task["task_id"]:
            errors.append(
                f"NEXT_TASK.md 指向 {id_match.group(1)}，"
                f"但 PROGRESS.md 当前/下一任务是 {next_task['task_id']}"
            )
    else:
        next_task_content = next_task_path.read_text(encoding="utf-8")
        if "所有任务已完成" not in next_task_content:
            errors.append("PROGRESS.md 无待办任务，但 NEXT_TASK.md 未显示所有任务已完成")

    return len(errors) == 0, errors


def main():
    repo_root = Path(__file__).resolve().parent.parent
    progress_path = repo_root / "PROGRESS.md"
    next_task_path = repo_root / "NEXT_TASK.md"

    print(f"验证 {progress_path} ...")
    passed, errors = verify_progress(progress_path, next_task_path)

    if passed:
        counts = task_counts(parse_tasks(progress_path.read_text(encoding="utf-8")))
        print(f"✅ 验证通过！{counts['completed']}/{counts['total']} 任务完成")
        return 0
    else:
        print(f"🚫 验证失败（{len(errors)} 个问题）：")
        for err in errors:
            print(f"  - {err}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
