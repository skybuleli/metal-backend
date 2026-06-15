#!/usr/bin/env python3
from __future__ import annotations

import argparse
import json
from pathlib import Path


def read_ppm(path: Path) -> tuple[int, int, bytes]:
    data = path.read_bytes()
    magic, rest = data.split(b"\n", 1)
    dims_line, max_line, body = rest.split(b"\n", 2)
    if magic != b"P6":
        raise SystemExit(f"{path}: PPM 魔数错误")
    if max_line != b"255":
        raise SystemExit(f"{path}: PPM 最大值错误")
    width, height = map(int, dims_line.split())
    return width, height, body


def sample_rgb(body: bytes, width: int, x: int, y: int) -> tuple[int, int, int]:
    offset = (y * width + x) * 3
    return tuple(body[offset : offset + 3])  # type: ignore[return-value]


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--image", required=True)
    parser.add_argument("--atlas", required=True)
    parser.add_argument("--meta", required=True)
    args = parser.parse_args()

    image_path = Path(args.image)
    atlas_path = Path(args.atlas)
    meta_path = Path(args.meta)

    width, height, body = read_ppm(image_path)
    atlas_w, atlas_h, atlas_body = read_ppm(atlas_path)
    if (width, height) != (960, 540):
        raise SystemExit(f"主图尺寸错误: {(width, height)}")
    if (atlas_w, atlas_h) != (64, 64):
        raise SystemExit(f"atlas 尺寸错误: {(atlas_w, atlas_h)}")

    board = sample_rgb(body, width, 132, 260)
    court = sample_rgb(body, width, 708, 260)
    hud = sample_rgb(body, width, 180, 42)
    bg = sample_rgb(body, width, 12, 20)

    if board == bg:
        raise SystemExit("Tetris 棋盘未形成与背景的可见差异")
    if court == bg:
        raise SystemExit("Pong 球场未形成与背景的可见差异")
    if hud == bg:
        raise SystemExit("HUD 未形成与背景的可见差异")
    if board == court:
        raise SystemExit("Tetris 与 Pong 视觉区分不明显")
    if atlas_body[:3] == b"\x00\x00\x00":
        raise SystemExit("atlas 似乎未写入")

    meta = {
        "task": "P4.6.8",
        "demo": "P4_6c",
        "scene": "lightweight homebrew smoke proxy for TetrisNX and Pong-NX",
        "artifacts": [
            "docs/evidence/P4.6.8-p4-6c-homebrew-smoke.ppm",
            "docs/evidence/P4.6.8-p4-6c-homebrew-smoke.png",
            "docs/evidence/P4.6.8-p4-6c-homebrew-atlas.ppm",
            "docs/evidence/P4.6.8-p4-6c-homebrew-atlas.png",
            "docs/evidence/P4.6.8-run.txt",
        ],
        "samples": {
            "board": board,
            "court": court,
            "hud": hud,
            "background": bg,
        },
        "all_passed": True,
    }
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"P4.6.8 证据校验通过: board={board}, court={court}, hud={hud}, bg={bg}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
