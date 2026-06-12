# D6 — Advanced Lighting

> `P2.8` 目标：在 D5 的基础上补齐 `Shadow Map + HDR 渲染 + Tone Mapping + Bloom 后处理`，并把多 pass 链路固化为可重复验证的离屏证据。

## 当前实现

- **Shadow Map**：先执行单独的 depth pass，生成 1024x1024 阴影贴图
- **HDR 主场景**：场景先写入 `RGBA16Float` 纹理，再进入后处理
- **Tone Mapping**：最终合成时对 HDR 颜色做曝光与 gamma 映射
- **Bloom**：从 HDR 场景提取高亮区域，执行横向与纵向两次 blur，再与主图合成
- **多物体场景**：地面 + 多个立方体 + 发光立方体，方便同时观察阴影与高亮溢出
- **手写 MSL**：继续使用运行时编译的内嵌 MSL，避免把重点转移到 Path A 兼容性

## 构建

```bash
make -C src/demos/d6 build
```

## 运行

```bash
make -C src/demos/d6 run
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
| `docs/evidence/P2.8-run.txt` | 运行日志，包含 shadow / HDR / bloom / composite 四段 pass |
| `docs/evidence/P2.8-d6-advanced-lighting.png` | 最终合成截图 |
| `docs/evidence/P2.8-meta.json` | 元数据 |
