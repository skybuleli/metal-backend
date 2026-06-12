# D6 — Advanced Lighting

> `P2.8 / P2.8a` 目标：在 D5 的基础上补齐 `Shadow Map + HDR 渲染 + Tone Mapping + Bloom 后处理`，并把核心场景 pass 逐步切到 Path A。

## 当前实现

- **Shadow Map**：先执行单独的 depth pass，生成 1024x1024 阴影贴图
- **HDR 主场景**：场景先写入 `RGBA16Float` 纹理，再进入后处理
- **Tone Mapping**：最终合成时对 HDR 颜色做曝光与 gamma 映射
- **Bloom**：从 HDR 场景提取高亮区域，执行横向与纵向两次 blur，再与主图合成
- **多物体场景**：地面 + 多个立方体 + 发光立方体，方便同时观察阴影与高亮溢出
- **混合着色器路径**：Shadow pass 与 HDR scene pass 默认走 `Slang -> DXIL -> MSC -> metallib`，Bloom 与 Tone Mapping 继续保留手写 MSL
- **对照模式**：可用 `--legacy-msl` 切回原始手写 MSL 场景 pass，为后续双路径比对保留基线

## 构建

```bash
make -C src/demos/d6 build
```

## 运行

```bash
make -C src/demos/d6 run
make -C src/demos/d6 run ARGS=--legacy-msl
```

## 生成验证证据

```bash
make -C src/demos/d6 evidence
```

## 当前取舍

- 这一版优先把多 pass 渲染链路打通，因此选择了 `方向光阴影 + 单层 bloom blur` 的稳妥组合。
- `PBR`、更复杂的 bloom 金字塔、软阴影抖动和实时窗口版留给后续 D8 或性能阶段处理。

## 证据产物

| 文件 | 说明 |
|------|------|
| `docs/evidence/P2.8-run.txt` | 手写 MSL 基线版本的运行日志 |
| `docs/evidence/P2.8-d6-advanced-lighting.png` | 手写 MSL 基线截图 |
| `docs/evidence/P2.8-meta.json` | 手写 MSL 基线元数据 |
| `docs/evidence/P2.8a-run.txt` | Path A 场景 pass 运行日志 |
| `docs/evidence/P2.8a-d6-patha-advanced-lighting.png` | Path A 场景 pass 截图 |
| `docs/evidence/P2.8a-meta.json` | Path A 元数据与反射文件索引 |
