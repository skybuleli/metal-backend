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


def verify_progress(progress_path: Path) -> tuple[bool, list[str]]:
    """验证 PROGRESS.md。返回 (是否通过, 错误列表)。"""
    errors = []

    if not progress_path.exists():
        errors.append(f"文件不存在：{progress_path}")
        return False, errors

    content = progress_path.read_text(encoding="utf-8")

    # 检查 1：不能有两个 🔄 同时存在
    in_progress = re.findall(r"🔄", content)
    if len(in_progress) > 1:
        errors.append(f"发现 {len(in_progress)} 个 🔄（进行中）任务，同时只能有一个！")
    elif len(in_progress) == 0:
        pass  # 可以没有进行中的任务

    # 检查 2：任务总数与进度百分比
    # 匹配 | P0.1 | ... | ✅ | 或类似格式
    all_tasks = re.findall(r"\|\s*(P\d+\.\d+[a-z]?)\s*\|", content)
    completed_tasks = re.findall(r"\|\s*(P\d+\.\d+[a-z]?)\s*\|.*?\|\s*✅\s*\|", content)

    total = len(set(all_tasks))
    completed = len(set(completed_tasks))

    if total == 0:
        errors.append("未找到任何任务行（格式：| P0.1 | 描述 | 状态 |）")
    else:
        # 检查完成的百分比
        reported_pct = re.search(r"(\d+\.?\d*)%", content)
        if reported_pct:
            actual_pct = round(completed / total * 100, 1)
            reported_val = float(reported_pct.group(1))
            if abs(actual_pct - reported_val) > 1:
                errors.append(
                    f"百分比不一致：报告中 {reported_val}%，实际 {actual_pct}%"
                    f"（{completed}/{total}）"
                )

    # 检查 3：重复的任务 ID
    task_ids = [m.group(1) for m in re.finditer(r"\|\s*(P\d+\.\d+[a-z]?)\s*\|", content)]
    seen = {}
    for tid in task_ids:
        if tid in seen:
            errors.append(f"重复的任务 ID：{tid}")
        seen[tid] = True

    # 检查 4：✅ 任务必须有日期
    completed_lines = re.findall(
        r"\|\s*(P\d+\.\d+[a-z]?)\s*\|.*?\|\s*✅\s*\|.*?\|",
        content,
    )
    for line_match in re.finditer(
        r"\|\s*(P\d+\.\d+[a-z]?)\s*\|(.*?)\|\s*✅\s*\|(.*?)\|",
        content,
    ):
        date_col = line_match.group(3).strip()
        if not re.search(r"\d{4}-\d{2}-\d{2}", date_col):
            errors.append(
                f"{line_match.group(1)} 标记为 ✅ 但没有完成日期"
            )

    return len(errors) == 0, errors


def main():
    repo_root = Path(__file__).resolve().parent.parent
    progress_path = repo_root / "PROGRESS.md"

    print(f"验证 {progress_path} ...")
    passed, errors = verify_progress(progress_path)

    if passed:
        total = len(re.findall(r"\|\s*P\d+\.\d+[a-z]?\s*\|", progress_path.read_text()))
        completed = len(re.findall(r"\|\s*P\d+\.\d+[a-z]?\s*\|.*?\|\s*✅\s*\|", progress_path.read_text()))
        print(f"✅ 验证通过！{completed}/{total} 任务完成")
        return 0
    else:
        print(f"🚫 验证失败（{len(errors)} 个问题）：")
        for err in errors:
            print(f"  - {err}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
