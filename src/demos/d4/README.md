# D4 — Basic Lighting

> `P2.6` 目标：uniform buffer + 3D 变换(MVP) + 深度测试 + 背面剔除 + Phong 光照。

## 当前实现

- **手写 MSL**（内嵌在 `src/main.cpp`）：编译自源代码，规避 Path A（Slang→DXIL→MSC）的 `mul`/`dot` 限制
- **顶点着色器**：从 vertex buffer 读取 position + normal，通过 uniform buffer 的 MVP 矩阵做 3D 变换
- **片段着色器**：Phong 光照模型（ambient + diffuse + specular），光源/相机位置从 uniform buffer 传入
- **空间一致性**：顶点阶段先用 `modelMatrix` 生成 world position / world normal，再在片段阶段按 world space 做光照
- **顶点缓冲**：36 顶点立方体（6 面 × 2 三角 × 3 顶点），position 和 normal 分开绑定到 buffer(0) 和 buffer(1)
- **Uniform Buffer**：`UniformData`（160 字节，含 `mvpMatrix + modelMatrix + lightPos + cameraPos`），VS @ buffer(2)，FS @ buffer(0)
- **深度测试**：Depth32Float 纹理 + CompareLess + WriteEnabled
- **背面剔除**：CullModeBack + WindingCounterClockwise
- **离屏渲染**：输出 `out/basic_lighting.ppm`（512 × 512）
- **实时窗口版（P2.6b）**：`NSWindow + CAMetalLayer` 持续渲染自动旋转立方体，用于人眼确认 3D 运动与光照变化

## 构建

```bash
make -C src/demos/d4 build
make -C src/demos/d4 build-window
```

## 运行

```bash
make -C src/demos/d4 run
make -C src/demos/d4 run-window
```

## 生成验证证据

```bash
make -C src/demos/d4 evidence
make -C src/demos/d4 evidence-window
```

## 技术说明

D4 采用手写 MSL 而非 Path A（Slang→DXIL→MSC）管线，因为：
- slangc DXIL 后端的 `mul(float4, float4x4)` 在顶点数 > 3 时导致渲染失败
- 手写 MSL 通过 `device->newLibrary(source, ...)` 在运行时编译，无此限制
- 路径选择符合 ADR-004 回退策略

## 当前限制

- 当前输出仍是单样本离屏渲染，因此立方体边缘会有可见锯齿。
- 实时窗口版当前也是单样本渲染，因此旋转时边缘锯齿会更明显。
- 抗锯齿（例如 MSAA）更适合放到 `P2.7 / D5` 的高级贴图与 RTT/MSAA 子集里统一处理。

## 证据产物

| 文件 | 说明 |
|------|------|
| `docs/evidence/P2.6-run.txt` | 运行日志 |
| `docs/evidence/P2.6-d4-basic-lighting.png` | 渲染截图 |
| `docs/evidence/P2.6-meta.json` | 元数据 |
| `docs/evidence/P2.6b-window.txt` | 实时窗口运行日志 |
| `docs/evidence/P2.6b-d4-rotating-window.png` | 实时窗口最终帧导出图 |
| `docs/evidence/P2.6b-meta.json` | 实时窗口元数据 |
