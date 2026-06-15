# P4.6.10 - 窗口版 2D smoke

## 目标

这个样本不再只看离屏输出，而是把一张 2D smoke 画面通过桥接层 Presenter 真正送进 `NSWindow + CAMetalLayer`。

它主要用于验证：

- 窗口是否能正常创建和显示
- `metal_create_presenter` / `metal_presenter_resize` / `metal_presenter_present_texture` 是否连通
- CPU 生成的 2D 画面能否稳定上传到桥接纹理，再被窗口呈现

## 画面内容

- 左侧 Tetris 风格棋盘
- 右侧 Pong 风格球场
- 顶部 HUD
- 底部状态条

## 构建

```bash
make -C src/demos/p4_6d build
```

## 运行

```bash
make -C src/demos/p4_6d run
```

## 生成证据

```bash
make -C src/demos/p4_6d evidence
```

证据会输出到：

- `docs/evidence/P4.6.10-run.txt`
- `docs/evidence/P4.6.10-window-smoke.ppm`
- `docs/evidence/P4.6.10-window-smoke.png`
- `docs/evidence/P4.6.10-meta.json`
