# P4.6.8 - 轻量 2D homebrew smoke

## 范围

这个样本不是完整游戏，而是把 `TetrisNX` / `Pong-NX` 这一档轻量 homebrew 的视觉特征压缩成一个可验证的首帧：

- 左侧 Tetris 风格棋盘
- 右侧 Pong 风格球场
- 顶部 HUD 标题和副标题

## 目标

目标是确认当前 2D 闭环已经能稳定承载轻量 homebrew 常见的画面结构：

1. 规则化 tile 棋盘
2. 简单球场与挡板
3. HUD / 标题栏
4. 透明面板叠加

## 构成

- 单张 atlas 同时承载背景、方块、挡板、球体和字体 glyph
- 第一帧固定输出，作为 smoke 证据
- 画面分区明确，便于肉眼判断是否满足 smoke 级别

## 构建

```bash
make -C src/demos/p4_6c evidence
```

## 证据

- `docs/evidence/P4.6.8-p4-6c-homebrew-smoke.ppm`
- `docs/evidence/P4.6.8-p4-6c-homebrew-smoke.png`
- `docs/evidence/P4.6.8-p4-6c-homebrew-atlas.ppm`
- `docs/evidence/P4.6.8-p4-6c-homebrew-atlas.png`
- `docs/evidence/P4.6.8-run.txt`
- `docs/evidence/P4.6.8-meta.json`
