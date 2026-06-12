# D7 — GPU-Driven

> `P2.9` 目标：用 `Path A compute + instancing + indirect draw` 做一版真正由 GPU 驱动的粒子场景，并留下截图、运行日志和性能记录。

## 当前实现

- **Compute 更新**：粒子状态由 `Slang -> DXIL -> MSC -> metallib` 的 compute shader 每帧更新
- **Indirect Draw**：compute shader 同时写入 `MTLDrawPrimitivesIndirectArguments` 布局的参数缓冲
- **Instancing 渲染**：渲染阶段使用 instanced quad，每个实例读取一个粒子
- **CPU 提交量固定**：每帧只有 `1 次 compute dispatch + 1 次 indirect draw`
- **性能记录**：运行结束会输出平均 FPS、平均帧时间，以及 indirect `instanceCount` 的变化范围

## 构建

```bash
make -C src/demos/d7 build
```

## 运行

```bash
make -C src/demos/d7 run
```

## 生成验证证据

```bash
make -C src/demos/d7 evidence
```

## 证据产物

| 文件 | 说明 |
|------|------|
| `docs/evidence/P2.9-run.txt` | 运行日志，包含 compute / indirect draw / 性能摘要 |
| `docs/evidence/P2.9-d7-gpu-driven.png` | 最终粒子画面截图 |
| `docs/evidence/P2.9-perf.json` | 平均 FPS、平均帧时间、instanceCount 范围 |
| `docs/evidence/P2.9-compute-reflection.json` | Path A compute 反射文件 |
| `docs/evidence/P2.9-meta.json` | D7 元数据 |
