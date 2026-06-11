# D2 — Textured Quad

> `P2.3` 目标：把 `Path A` 真正接入 Demo，使用 `Slang → DXIL → MSC → metallib` 编译着色器，并渲染一个带纹理采样的离屏 quad。

## 当前实现

- `shaders/quad.slang` 提供顶点与片段入口
- `make build` 时执行 `slangc` 和 `metal-shaderconverter`
- 生成 `build/quad_vertex.metallib` 与 `build/quad_fragment.metallib`
- 同步生成 `build/quad_vertex.reflect.json` 与 `build/quad_fragment.reflect.json`
- C++ 运行时从 `metallib` 加载着色器
- 运行时创建 2x2 测试纹理，并按 `metal_irconverter_runtime` 的参数缓冲约定绑定纹理/采样器
- 将结果写出为 `out/textured_quad.ppm`

## 构建

```bash
make -C src/demos/d2 build
```

## 运行

```bash
make -C src/demos/d2 run
```

## 生成验证证据

```bash
make -C src/demos/d2 evidence
```

该命令会：

- 重新构建并运行 `D2`
- 复制 `out/textured_quad.ppm` 到 `docs/evidence/`
- 自动转换出 `docs/evidence/P2.4-d2-textured-quad.png`
- 生成运行日志 `docs/evidence/P2.4-run.txt`
- 生成元数据 `docs/evidence/P2.4-meta.json`
- 复制顶点/片段反射文件到 `docs/evidence/`

## 预期产物

- 可执行文件：`src/demos/d2/build/d2_textured_quad`
- DXIL：`src/demos/d2/build/quad_vertex.dxil`、`src/demos/d2/build/quad_fragment.dxil`
- metallib：`src/demos/d2/build/quad_vertex.metallib`、`src/demos/d2/build/quad_fragment.metallib`
- 反射：`src/demos/d2/build/quad_vertex.reflect.json`、`src/demos/d2/build/quad_fragment.reflect.json`
- 离屏图像：`src/demos/d2/out/textured_quad.ppm`
- 人工查看图像：`docs/evidence/P2.4-d2-textured-quad.png`

## 说明

- 当前阶段重点是把 `Path A` 和纹理采样接通，因此仍然沿用离屏渲染。
- `P2.4` 额外固化了 PNG、运行日志和 MSC 反射文件，方便后续继续扩展到多纹理与采样器状态。
