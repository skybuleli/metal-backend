# D5 — Advanced Texturing

> `P2.7` 目标：在 `D4` 的基础上补上高级贴图能力，优先打通 `法线贴图 + Skybox/Cubemap + MSAA` 这一条最有展示价值的子集。

## 当前实现

- **程序化 Skybox/Cubemap**：6 个面运行时生成，不依赖外部图片资源
- **法线贴图平面**：前景为带切线空间法线贴图的倾斜平面，可明显看到凹凸光照变化
- **环境反射混合**：前景片段会混入少量 cubemap 反射，证明高级纹理读取链路已经贯通
- **4x MSAA**：离屏颜色附件使用多重采样，最终 resolve 到单样本纹理导出
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

- `P2.7` 先交付 `法线贴图 + Skybox/Cubemap + MSAA`，因为这组能力最直接对应“高级贴图”视觉收益。
- `RTT` 没有在这一版强行塞进来，避免把任务做成两套半成品；后续若 `D6` 需要后处理链路，再引入 RTT 会更顺。

## 证据产物

| 文件 | 说明 |
|------|------|
| `docs/evidence/P2.7-run.txt` | 运行日志 |
| `docs/evidence/P2.7-d5-advanced-texturing.png` | 渲染截图 |
| `docs/evidence/P2.7-meta.json` | 元数据 |
