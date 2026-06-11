# GAL→Metal 映射速查（Phase 3+）

> Ryubing IPipeline 接口 → Metal API 核心映射。实现 MetalPipeline 时直接引用。

## 核心映射表

| GAL IPipeline 方法组 | Metal 对应 API |
|----------------------|----------------|
| `SetProgram` | `setRenderPipelineState` / `setComputePipelineState` |
| `SetRenderTargets` | `MTLRenderPassDescriptor` |
| `SetViewports` / `SetScissors` | `setViewport` / `setScissorRect` |
| `SetVertexBuffers` / `SetIndexBuffer` | `setVertexBuffer` / `drawIndexedPrimitives` |
| `SetUniformBuffers` / `SetStorageBuffers` | `setVertexBuffer` / `setFragmentBuffer` |
| `SetTextureAndSampler` | `setFragmentTexture` + `setFragmentSamplerState` |
| `SetImage` | `setFragmentTexture` |
| `SetBlendState` | `MTLRenderPipelineColorAttachment.blending` |
| `SetDepthTest` / `SetStencilTest` | `MTLDepthStencilState` |
| `SetFaceCulling` / `SetFrontFace` | `setCullMode` / `setFrontFacingWinding` |
| `SetPolygonMode` | `setTriangleFillMode` |
| `SetPrimitiveTopology` | 影响 `drawPrimitives` 的 primitiveType |
| `SetDepthBias` | `setDepthBias` / `setDepthSlopeScale` / `setDepthClamp` |
| `SetMultisampleState` | `MTLRenderPipelineDescriptor` 的 sampleCount |
| `Draw` / `DrawIndexed` | `drawPrimitives` / `drawIndexedPrimitives` |
| `DrawIndirect` | `drawPrimitives:indirectBuffer:` |
| `DispatchCompute` | `dispatchThreadgroups` (MTLComputeCommandEncoder) |
| `ClearRenderTarget*` | `MTLRenderPassDescriptor` loadAction = Clear |
| `CopyBuffer` | `MTLBlitCommandEncoder.copyFromBuffer` |
| `Barrier` / `CommandBufferBarrier` | `MTLFence` / `waitUntilCompleted` |
| `BeginTransformFeedback` | 需 Compute Shader 模拟（Metal 无原生 TF） |
| `TryHostConditionalRendering` | `MTLCounterSampleBuffer`（Metal 3.0+） |

## 接口定义来源

完整的 63 个方法定义位于 Ryubing 源码：
- `src/Ryujinx.Graphics.GAL/IPipeline.cs`
- 本地路径：`~/dev/ryubing/src/Ryujinx.Graphics.GAL/IPipeline.cs`

## Phase 3 实现策略

1. 先创建所有方法的 stub（返回空或抛出 NotImplementedException）
2. 确保编译通过
3. Phase 4/5 逐步实现每个方法
