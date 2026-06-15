# P4.6.11 - 真实 draw path smoke

## 目标

这个样本用 `libmetal_bridge` 自己编译 shader、创建 pipeline、编码 draw call，然后把渲染结果送进窗口。

它主要用于验证：

- `metal_compile_shader` 能否从 Slang 原生源码产出 metallib
- `metal_create_render_pipeline` 能否创建可用的渲染管线
- `metal_begin_render_encoding_with_targets` + `metal_render_encoder_draw_primitives` 能否真正出图
- `metal_texture_readback` 能否把真实渲染结果读回证据文件
- `metal_presenter_present_texture` 能否把同一结果送进窗口

## 画面内容

- 一个由桥接层管线绘制的彩色三角形
- 渲染目标回读用于生成 PPM / JSON 证据

## 构建

```bash
make -C src/demos/p4_6e build
```

## 运行

```bash
make -C src/demos/p4_6e run
```

## 生成证据

```bash
make -C src/demos/p4_6e evidence
```

证据会输出到：

- `docs/evidence/P4.6.11-run.txt`
- `docs/evidence/P4.6.11-bridge-triangle.ppm`
- `docs/evidence/P4.6.11-bridge-triangle.png`
- `docs/evidence/P4.6.11-meta.json`
