# D4 — Basic Lighting

> `P2.6` 目标：验证 Phong 光照模型（ambient + diffuse + specular）通过 Path A（Slang→DXIL→MSC→metallib）渲染。

## 当前实现

- `shaders/lighting.slang` 提供顶点与片段入口
- 顶点着色器：pass-through 位置和法线（不做 MVP 变换，因 slangc→DXIL 管线限制）
- 片段着色器：Phong 光照模型（ambient + diffuse + specular）
- `make build` 时执行 `slangc` 和 `metal-shaderconverter`
- 生成 `build/lighting_vertex.metallib` 与 `build/lighting_fragment.metallib`
- 离屏渲染输出 `out/basic_lighting.ppm`

## 已知限制（ slangc→DXIL→MSC 管线）

1. **`mul(float4, float4x4)` 在顶点数 > 3 时导致渲染失败**
2. **`dot()` 内建函数同样受影响**
3. **静态数组大小限制为 ≤6 元素**
4. **顶点输出超过 2 个插值器时，复杂计算会失败**

**解决方案**：顶点着色器仅做简单 pass-through 或缩放，所有光照计算在片段着色器完成。

## 构建

```bash
make -C src/demos/d4 build
```

## 运行

```bash
make -C src/demos/d4 run
```

## 生成验证证据

```bash
make -C src/demos/d4 evidence
```
