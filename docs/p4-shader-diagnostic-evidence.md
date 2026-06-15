# P4.6.4 着色器诊断证据链

## 目标

把着色器排查从“单次编译结果”升级成“可归档、可复现、可对照”的诊断包，确保以后任何一次失败都能明确归因。

## 证据包结构

每个样本建议生成一个独立目录，目录内至少包含：

```text
<case>/
  source_dump.slang
  slang_output.log
  dxil.bin
  dxil.summary.json
  metallib.bin
  metallib.summary.json
  reflection.json
  diagnosis.json
```

## 归因标签

诊断包里的 `diagnosis.json` 至少应包含以下标签：

- `source_ok`
- `slang_ok`
- `dxil_ok`
- `msc_ok`
- `metallib_ok`
- `slang_failed`
- `msc_failed`
- `missing_input`
- `empty_dxil`
- `empty_metallib`
- `bridge_disabled`

## 判读顺序

1. 先看 `source_dump.slang` 是否与预期样本一致。
2. 再看 `slang_output.log`，确认是 Slang 阶段失败还是后续阶段失败。
3. 再看 `dxil.summary.json`，确认 DXIL 是否有产出、大小是否合理、hash 是否稳定。
4. 再看 `metallib.summary.json` 和 `reflection.json`，确认 MSC 输出是否有效。
5. 最后看 `diagnosis.json`，确认失败标签是否符合当前桥接策略。

## 使用原则

- 主路径样本优先用 Slang 原生语法。
- GLSL 只在显式诊断桥接开关开启时采样。
- 失败包必须保留，不要只保留成功样本。
- 证据包应和源码仓库中的模板或输入文件一一对应，避免离线时失去上下文。
