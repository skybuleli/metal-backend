# D4 — Basic Lighting

> `P2.6` 目标：uniform buffer + 3D 变换(MVP) + 深度测试 + 背面剔除 + Phong 光照。

## 当前实现

- **手写 MSL**（内嵌在 `src/main.cpp`）：编译自源代码，规避 Path A（Slang→DXIL→MSC）的 `mul`/`dot` 限制
- **顶点着色器**：从 vertex buffer 读取 position + normal，通过 uniform buffer 的 MVP 矩阵做 3D 变换
- **片段着色器**：Phong 光照模型（ambient + diffuse + specular），光源/相机位置从 uniform buffer 传入
- **顶点缓冲**：36 顶点立方体（6 面 × 2 三角 × 3 顶点），position 和 normal 分开绑定到 buffer(0) 和 buffer(1)
- **Uniform Buffer**：`UniformData`（96 字节，含 mvpMatrix + lightPos + cameraPos），VS @ buffer(2)，FS @ buffer(0)
- **深度测试**：Depth32Float 纹理 + CompareLess + WriteEnabled
- **背面剔除**：CullModeBack + WindingCounterClockwise
- **离屏渲染**：输出 `out/basic_lighting.ppm`（512 × 512）

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

## 技术说明

D4 采用手写 MSL 而非 Path A（Slang→DXIL→MSC）管线，因为：
- slangc DXIL 后端的 `mul(float4, float4x4)` 在顶点数 > 3 时导致渲染失败
- 手写 MSL 通过 `device->newLibrary(source, ...)` 在运行时编译，无此限制
- 路径选择符合 ADR-004 回退策略

## 证据产物

| 文件 | 说明 |
|------|------|
| `docs/evidence/P2.6-run.txt` | 运行日志 |
| `docs/evidence/P2.6-d4-basic-lighting.png` | 渲染截图 |
| `docs/evidence/P2.6-meta.json` | 元数据 |
