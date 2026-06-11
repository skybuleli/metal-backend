# D3 — Multi-Texture

> `P2.5` 目标：在 `Path A` 基础上扩展到双纹理、双采样器状态，并验证 `repeat/clamp`、`linear/nearest` 与 `mipLinear/notMipmapped` 组合能稳定通过 MSC → `metallib` 渲染链路。

## 当前实现

- `shaders/multi_texture.slang` 提供顶点与片段入口
- `make build` 时执行 `slangc` 和 `metal-shaderconverter`
- 生成 `build/multi_texture_vertex.metallib` 与 `build/multi_texture_fragment.metallib`
- 同步生成 `build/multi_texture_vertex.reflect.json` 与 `build/multi_texture_fragment.reflect.json`
- C++ 运行时创建一张带 mipmap 的主纹理和一张叠加纹理
- 主采样器使用 `repeat + linear + mipLinear`
- 叠加采样器使用 `clamp + nearest + notMipmapped`
- 渲染结果写出为 `out/multi_texture.ppm`

## 构建

```bash
make -C src/demos/d3 build
```

## 运行

```bash
make -C src/demos/d3 run
```

## 生成验证证据

```bash
make -C src/demos/d3 evidence
```

该命令会：

- 重新构建并运行 `D3`
- 复制 `out/multi_texture.ppm` 到 `docs/evidence/`
- 自动转换出 `docs/evidence/P2.5-d3-multi-texture.png`
- 生成运行日志 `docs/evidence/P2.5-run.txt`
- 生成元数据 `docs/evidence/P2.5-meta.json`
- 复制顶点/片段反射文件到 `docs/evidence/`

## 预期产物

- 可执行文件：`src/demos/d3/build/d3_multi_texture`
- DXIL：`src/demos/d3/build/multi_texture_vertex.dxil`、`src/demos/d3/build/multi_texture_fragment.dxil`
- metallib：`src/demos/d3/build/multi_texture_vertex.metallib`、`src/demos/d3/build/multi_texture_fragment.metallib`
- 反射：`src/demos/d3/build/multi_texture_vertex.reflect.json`、`src/demos/d3/build/multi_texture_fragment.reflect.json`
- 离屏图像：`src/demos/d3/out/multi_texture.ppm`
- 人工查看图像：`docs/evidence/P2.5-d3-multi-texture.png`

## 说明

- 当前阶段重点是多纹理混合和采样器状态，不引入额外的 3D 相机或深度测试逻辑。
- D3 延续 D2 的 MSC 顶层参数缓冲绑定方式，为后续 D4 的 uniform buffer 和更复杂资源绑定打基础。
