# Demo Preview Window

> `P2.6a` 目标：在不引入完整 Presenter/交换链复杂度的前提下，为 `D1-D4` 提供一个本地可见的 `NSWindow + CAMetalLayer` 预览窗口。

## 当前实现

- 使用 `NSWindow` 创建 1024×1024 本地预览窗口
- 使用 `CAMetalLayer` 呈现四宫格预览
- 预览器本身使用 Objective-C++ 直接调用 Cocoa/Metal，不引入 P4 的 Presenter 复杂度
- 加载 `D1-D4` 各自的离屏 `PPM` 产物作为纹理源
- 保留各 Demo 原有离屏证据链，不修改其核心渲染路径
- 支持自动关闭和导出组合预览图，便于生成证据

## 构建

```bash
make -C src/demos/preview build
```

## 运行

```bash
make -C src/demos/preview run
```

## 生成验证证据

```bash
make -C src/demos/preview evidence
```

该命令会：

- 先确保 `D1-D4` 的离屏输出已生成
- 启动本地窗口预览
- 自动关闭窗口
- 导出 `docs/evidence/P2.6a-preview-grid.ppm` 与 `png`
- 生成窗口运行日志 `docs/evidence/P2.6a-window.txt`

## 说明

- 这个预览窗口是 P2 阶段的轻量体验增强，不等同于 P4 的完整 Presenter。
- `D1-D4` 仍然以各自的离屏渲染结果为真实验证来源；窗口预览主要服务于人眼观察和日常调试。
