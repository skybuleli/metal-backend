# P4.6.6 手写 2D 最小样本 A

## 范围

这个样本用于验证 2D 最小闭环里的第一块：

- 单个 atlas 纹理
- 一个背景 quad
- 一个前景 sprite quad
- alpha blend

## 设计

- 纹理使用 2x2 atlas
- 背景 quad 采样左上角 tile
- 前景 sprite 采样右下角 tile
- 前景开启 `sourceAlpha / oneMinusSourceAlpha` 混合

## 证据

- 运行日志会记录 atlas 布局、像素采样和混合模式
- 输出图像会同时导出 scene 和 atlas

