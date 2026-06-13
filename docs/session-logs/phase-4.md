# Phase 4 会话日志

## 阶段摘要

- **阶段**: Phase 4 — 核心 Metal 后端实现
- **当前状态**: 进行中
- **归档范围**: 主日志中较早的 Phase 4 记录

---

## 2026-06-13 | P4.2.0 MSC/Metal 着色器能力验证矩阵 — 4 维度 35/35 全部通过 | ✅ 完成

- **Agent**: Codex (Buffy)
- **结果**: ✅ 建立 MSC/Metal 限制验证矩阵，覆盖纹理/discard/subgroup/helper 四个维度，35/35 Path A 编译测试全部通过
- **关键发现**:
  - `SV_IsHelperInvocation` 在 Slang→DXIL 路径中不受支持
  - 如需 `isHelper` 能力，CommandMapper 应在 MSL 层处理 `[[is_helper_invocation]]`

---

## 2026-06-13 下午 | P4.1.0 固化 Metal 硬件限制常量与资源对齐策略 | ✅ 完成

- **Agent**: Codex (Buffy)
- **结果**: ✅ 已创建 `metal_limits.h`，固化 Metal 硬件限制常量、对齐要求和存储模式选择策略

---

## 2026-06-13 下午 | P4.1.1 MetalDevice GPU 选择 + 设备创建 + 能力查询 + 单元测试 | ✅ 完成

- **Agent**: Codex (Buffy)
- **结果**: ✅ 完整实现 MetalDevice 的 GPU 选择、设备创建、能力查询和 MetalQueue 创建

---

## 2026-06-13 下午 | P4.1.2 MetalBuffer MTLStorageMode 策略 + 6 个 C ABI 函数 + Catch2 测试 | ✅ 完成

- **Agent**: Codex (Buffy)
- **结果**: ✅ 完整实现 MetalBuffer 的 6 个 C ABI 函数，并补齐 Catch2 测试

---

## 2026-06-13 晚间 | MetalDeviceCaps → HasUnifiedMemory 驱动 MetalBufferPool 存储模式决策 | ✅ 完成

- **Agent**: Codex (Buffy)
- **结果**: ✅ 修复 MetalDeviceCaps → 存储模式决策链路

---

## 2026-06-13 | MetalTextureViewProxy.GetData 审查结论：不需要修改 | ✅ 完成

- **Agent**: Codex (Buffy)
- **结果**: ✅ 审查完成，确认 `MetalTextureViewProxy.GetData` 不需要与 `SetData(region)` 相同的父纹理 handle 改造

---

## 2026-06-13 | MetalTextureViewProxy 统一为父纹理 handle 模式 | ✅ 完成

- **Agent**: Codex (Buffy)
- **结果**: ✅ 将 `MetalTextureViewProxy` 全部三个数据操作方法统一为父纹理 handle 模式
