# P4.6.7 手写 2D 最小样本 B

## 范围

这个样本承接 `P4.6.6`，继续把 2D 闭环收窄到更接近真实游戏的结构：

- tile map
- camera scroll
- HUD 面板
- HUD 文本

## 目标

目标不是做一个完整游戏，而是把最容易在后续 homebrew 和蔚蓝里出问题的几个维度拆开：

1. 世界层滚动是否正常
2. tile atlas 取样是否正确
3. HUD 覆盖层是否稳定
4. 文本 glyph 是否能从同一张 atlas 正常取样

## 结构

- 世界层：固定 tile map，使用程序化生成的 2D 地图
- 相机：两组 scroll 坐标，生成两帧对照证据
- HUD：半透明面板加大号像素字体
- 资源：单张 atlas 同时承载世界 tile、字体 glyph 和 UI 白块

## 证据策略

为了证明 camera scroll 真的生效，样本会输出两帧：

- `scroll_a`
- `scroll_b`

再配合 atlas 和运行日志，形成一个完整的可视诊断链。

## 预期作用

这个样本跑通后，下一步就可以更稳地去碰轻量 homebrew：

- `TetrisNX`
- `Pong-NX`
- `OpenSupaplex`

如果这里都不稳定，说明问题更可能还在 shader、pipeline 或 bridge 的基础层，而不是蔚蓝本身。
