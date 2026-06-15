# P4.6.8 轻量 2D homebrew smoke 设计说明

## 目的

这一步不是直接碰蔚蓝，而是先做一个更接近轻量 homebrew 的固定首帧样本，用来验证：

- 规则化 tile 棋盘是否稳定
- 简单球场是否稳定
- HUD 和文本叠加是否稳定
- 透明面板是否稳定

## 为什么这样设计

`TetrisNX` 和 `Pong-NX` 都属于特别适合做 smoke test 的样本：

- 场景复杂度不高
- 视觉特征很典型
- 首帧就能看出是否渲染正常
- 对 tile、sprite、HUD、alpha blend 的组合要求很直接

所以我们用一个手写的 proxy scene 把这两类样本的共性压缩起来，先确认当前 2D 链路在这个复杂度下是通的。

## 结构

- 左侧：Tetris 风格棋盘
- 右侧：Pong 风格球场
- 上方：HUD 和标题栏
- 资源：单 atlas 承载所有元件

## 预期用途

这个样本通过后，下一步就可以更有把握地去接真实 homebrew：

1. `TetrisNX`
2. `Pong-NX`
3. `OpenSupaplex`
4. `NXEngine-evo`

如果这一步失败，问题优先归因到 shader、pipeline 或桥接，而不是直接怀疑蔚蓝。
