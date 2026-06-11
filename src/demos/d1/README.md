# D1 — Hello Triangle

> `P2.1` 目标：用 `metal-cpp` 和手写 MSL 搭起最小 Metal 渲染管线，并输出一张离屏彩色三角形图像。

## 当前实现

- 创建设备与命令队列
- 内嵌手写 MSL 顶点/片段着色器
- 创建 `RenderPipelineState`
- 渲染到 256x256 离屏纹理
- 将结果写出为 `out/triangle.ppm`

## 构建

```bash
make -C src/demos/d1 build
```

如果 `metal-cpp` 不在默认位置，可覆盖：

```bash
make -C src/demos/d1 build METAL_CPP_DIR=/你的/metal-cpp
```

## 运行

```bash
make -C src/demos/d1 run
```

## 生成验证证据

```bash
make -C src/demos/d1 evidence
```

该命令会：

- 重新运行离屏渲染
- 复制 `out/triangle.ppm` 到 `docs/evidence/`
- 自动转换出 `docs/evidence/P2.2-d1-triangle.png`
- 生成运行说明 `docs/evidence/P2.2-run.txt`
- 生成元数据 `docs/evidence/P2.2-meta.json`

## 预期产物

- 可执行文件：`src/demos/d1/build/d1_triangle`
- 离屏图像：`src/demos/d1/out/triangle.ppm`
- 人工查看图像：`docs/evidence/P2.2-d1-triangle.png`

## 说明

- 当前选择离屏渲染而不是窗口呈现，是为了在 `P2.1` 先验证最小 Metal 管线和 `drawPrimitives` 路径。
- `P2.2` 再基于这份产物补正式运行证据、截图说明和 M1 验证记录。
