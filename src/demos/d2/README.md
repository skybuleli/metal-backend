# D2 — Textured Quad

> `P2.3` 目标：把 `Path A` 真正接入 Demo，使用 `Slang → DXIL → MSC → metallib` 编译着色器，并渲染一个带纹理采样的离屏 quad。

## 当前实现

- `shaders/quad.slang` 提供顶点与片段入口
- `make build` 时执行 `slangc` 和 `metal-shaderconverter`
- 生成 `build/quad_vertex.metallib` 与 `build/quad_fragment.metallib`
- C++ 运行时从 `metallib` 加载着色器
- 运行时创建 2x2 测试纹理并采样
- 将结果写出为 `out/textured_quad.ppm`

## 构建

```bash
make -C src/demos/d2 build
```

## 运行

```bash
make -C src/demos/d2 run
```

## 预期产物

- 可执行文件：`src/demos/d2/build/d2_textured_quad`
- DXIL：`src/demos/d2/build/quad_vertex.dxil`、`src/demos/d2/build/quad_fragment.dxil`
- metallib：`src/demos/d2/build/quad_vertex.metallib`、`src/demos/d2/build/quad_fragment.metallib`
- 离屏图像：`src/demos/d2/out/textured_quad.ppm`

## 说明

- 当前阶段重点是把 `Path A` 和纹理采样接通，因此仍然沿用离屏渲染。
- `P2.4` 再补 PNG、运行说明和纹理 quad 的正式验证证据。
