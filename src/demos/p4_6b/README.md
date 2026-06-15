# P4.6.7 - 手写 2D 最小样本 B

## 范围

这个样本用于验证 2D 最小闭环里的第二块：

- tile map 世界
- camera scroll
- HUD 面板
- HUD 文本

## 设计

- 单张 atlas 同时容纳世界 tiles、字体 glyph 和 UI 白块
- 世界使用固定 tile map 生成，camera scroll 在顶点阶段生效
- HUD 使用半透明面板和大号像素字体叠加在世界之上
- 通过两帧不同 scroll 位置，验证画面真的发生了位移

## 构建

```bash
make -C src/demos/p4_6b evidence
```

## 证据

- `docs/evidence/P4.6.7-p4-6b-tilemap-scroll-a.ppm`
- `docs/evidence/P4.6.7-p4-6b-tilemap-scroll-a.png`
- `docs/evidence/P4.6.7-p4-6b-tilemap-scroll-b.ppm`
- `docs/evidence/P4.6.7-p4-6b-tilemap-scroll-b.png`
- `docs/evidence/P4.6.7-p4-6b-tilemap-atlas.ppm`
- `docs/evidence/P4.6.7-p4-6b-tilemap-atlas.png`
- `docs/evidence/P4.6.7-run.txt`
- `docs/evidence/P4.6.7-meta.json`
