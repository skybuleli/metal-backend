# D5 — Advanced Texturing

> `P2.7` 先打通 `法线贴图 + Skybox/Cubemap + MSAA` 子集；`P2.7a` 再把画面调成更直观的展示版，并补上可直接比较的 MSAA 证据。

## 当前实现

- **连续天空盒 Cubemap**：6 个面运行时生成，但内容不再是纯色块，而是连续的天空、地平线、远景轮廓和太阳高光
- **双面板法线贴图对照**：左侧面板关闭法线扰动，右侧面板开启强法线贴图，同一镜头内就能看出差异
- **环境反射混合**：面板片段混入 cubemap 反射，证明高级纹理读取链路已经贯通
- **1x / 4x MSAA 对比导出**：主图保留 4x MSAA，同时额外输出一张并排对比图
- **手写 MSL**：继续采用运行时编译的内嵌 MSL，避免把验证重点分散到 Path A 的着色器兼容性上

## 构建

```bash
make -C src/demos/d5 build
```

## 运行

```bash
make -C src/demos/d5 run
```

## 生成验证证据

```bash
make -C src/demos/d5 evidence
```

## 当前取舍

- `P2.7a` 优先把“看得懂”这件事做扎实，因此先补充对照式画面和 MSAA 证据，而不是继续把 `RTT` 塞进 D5。
- `RTT` 仍留给后续更适合的 `D6` 后处理链路，避免当前任务同时背两套复杂度。

## 证据产物

| 文件 | 说明 |
|------|------|
| `docs/evidence/P2.7a-run.txt` | 运行日志，包含 1x/4x MSAA 差异统计 |
| `docs/evidence/P2.7a-d5-showcase.png` | 强化后的 D5 主截图 |
| `docs/evidence/P2.7a-d5-msaa-compare.png` | 斜边区域放大后的左 1x、右 4x MSAA 对比图 |
| `docs/evidence/P2.7a-meta.json` | 元数据 |
