# P4.6.5 渲染诊断证据链

## 目标

把“画面不对”拆成一条可回放、可归档、可对照的证据链，避免后续再用蔚蓝做唯一调试样本时把问题混成一团。

## 证据包结构

建议每次采集都生成一个独立目录：

```text
P4.6.5-diagnostic-bundle/
  render_target_dump.ppm
  presented_frame.ppm
  draw_order.log
  state_snapshot.json
  manifest.json
```

## 各文件含义

- `render_target_dump.ppm`: 呈现前的渲染目标抓图，作为“画对没画对”的主证据。
- `presented_frame.ppm`: `presentDrawable` 之后的抓图，作为“提交后实际帧”的对照证据。
- `draw_order.log`: 一帧内的渲染顺序日志，记录 `window pass`、`capture pass` 和 `present` 的先后关系。
- `state_snapshot.json`: 关键状态快照，记录尺寸、已呈现帧数、自动关闭时间与证据路径。
- `manifest.json`: 文件清单，便于后续脚本或人类快速确认 bundle 是否完整。

## 判读顺序

1. 先看 `draw_order.log`，确认渲染步骤顺序没有乱。
2. 再看 `state_snapshot.json`，确认抓图时的尺寸与帧号。
3. 再看 `render_target_dump.ppm`，确认呈现前的内容是否正确。
4. 再看 `presented_frame.ppm`，确认提交到窗口后的结果是否一致。
5. 最后看 `manifest.json`，确认整包没有缺文件。

## 使用方式

```bash
make -C src/demos/d4 evidence-diagnostic
```

生成结果会落在 `docs/evidence/P4.6.5-diagnostic-bundle/` 和 `docs/evidence/P4.6.5-render-diagnostic.log`。

