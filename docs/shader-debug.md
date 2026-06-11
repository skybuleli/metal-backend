# 着色器调试速查（Phase 1+）

## SPIR-V 验证（每次编译后强制执行）

```bash
spirv-val output.spv
```

常见失败：
- `"ID xxx does not dominate its use"` → GLSL 变量声明顺序
- `"Capability xxx is not allowed"` → Slang 目标参数不正确
- `"OpEntryPoint interfaces incompletely"` → 着色器接口绑定不完整

## 常见编译错误速查

| 错误 | 原因 | 修复 |
|------|------|------|
| slangc 无输出 | 缺少 `-profile sm_6_0` | 加 `-profile sm_6_0` |
| slangc "unknown target" | 目标名错误 | `slangc -targets` 查看 |
| MSC "unsupported DXIL" | DXIL 版本过高 | 尝试不同 shader model |
| spirv-cross MSL 报错 | 缺 BufferBlock | 加 `--msl-decoration-binding` |
| glslangValidator 语法错 | GLSL 不兼容 | GLSL 460 + Maxwell 兼容子集 |

## 着色器大小健康检查

- metallib ≈ 2× DXIL 大小（正常）
- metallib < DXIL → MSC 转换失败
- 典型：2840B DXIL → 5804B metallib

## 调试命令速查

```bash
spirv-dis input.spv -o output.spvasm     # 反汇编
spirv-opt -O input.spv -o output.spv     # 优化
spirv-cross input.spv --msl --msl-version 30000 --output output.msl
```
