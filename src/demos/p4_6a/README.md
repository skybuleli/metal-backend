# P4.6.6 - 手写 2D 最小样本 A

> 单纹理 Quad + Sprite Atlas + alpha blend。

## 目标

这个样本不走 Slang/MSC 路线，而是直接用手写 MSL 把最小 2D 画面跑通，验证：

- 纹理采样
- sprite atlas 子区域 UV
- alpha blend
- 证据导出

## 构成

- 背景 quad：采样 atlas 左上角 tile
- 前景 sprite quad：采样 atlas 右下角 tile
- 前景开启 `sourceAlpha / oneMinusSourceAlpha` 混合

## 生成证据

```bash
make -C src/demos/p4_6a evidence
```

产物会写入：

- `docs/evidence/P4.6.6-p4-6a-sprite-quad.ppm`
- `docs/evidence/P4.6.6-p4-6a-sprite-quad.png`
- `docs/evidence/P4.6.6-p4-6a-sprite-atlas.ppm`
- `docs/evidence/P4.6.6-p4-6a-sprite-atlas.png`
- `docs/evidence/P4.6.6-run.txt`
- `docs/evidence/P4.6.6-meta.json`

