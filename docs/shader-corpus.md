# 着色器语料策略

> 目的：把 P1 阶段发现的真实/近真实着色器来源沉淀为后续回归测试、单元测试和集成测试的输入边界。

## 当前正式语料

| 来源 | 当前用法 | 状态 | 说明 |
|------|----------|------|------|
| `~/autommes/deko3d_slang_poc/test_output/` | P1.8b 扩展语料扫描 | ✅ 使用 | 本地已有 Slang/MSL 产物，覆盖 `real_*`、`kirby_*`、`large_*`、`final_*`、`smp_*`、`batch*`、`deko3d*` 等样本族。脚本只读取本地文件，不把大语料复制进仓库。 |
| `~/Library/Application Support/Ryujinx/msl_dump/` | P1.6/P1.7 真实 VS/FS 侦察 | ✅ 使用 | 当前只有 Vertex/Fragment MSL，未发现 compute 样本。MSL→metallib 需要完整 Xcode，因此只做结构检查和记录。 |
| `~/Library/Application Support/Ryujinx/shader_dump/` | P1.6~P1.8 局部侦察 | ✅ 使用 | 当前样本很少，包含 `.metal`、`.spv`、`.spv.dis`。可作为 Path C/交叉验证线索。 |
| 内嵌 Slang 控制样本 | P1.8b 基础能力回归 | ✅ 使用 | 覆盖 VS、FS、CS、RWStructuredBuffer、groupshared、atomic、RWByteAddressBuffer、RWTexture2D。用于保证脚本不是只做侦察。 |
| 内嵌 GLSL 对照样本 | P1.8b 预期问题记录 | ✅ 使用 | GLSL std430/storage buffer 在 Path A 下触发不兼容，保留为 Path C 对照语料。 |

## 暂不进入正式证据的来源

| 来源 | 判断 | 后续动作 |
|------|------|----------|
| `maierfelix/bnsh-decoder` | 工具可采用，但必须由维护者提供合法 `.bnsh` 输入。仓库不保存 ROM 提取物。 | 等待合法样本后新增 `workfiles/` 本地说明或最小化哈希索引。 |
| RyuSAK 共享缓存 | 版权和来源灰区，不进入正式证据、CI 或仓库存档。 | 仅允许私下侦察问题形态；不能作为任务完成证据。 |
| Khronos GLSL 规范示例 | 可采用，但应最小化改写为内嵌控制样本。 | 后续拆成 P6 单元/集成测试用例。 |
| Mesa shader-db / nouveau 测试集 | 可采用，但不应整库 vendoring。 | 后续抽取最小失败样本，并记录上游路径、许可证和触发特征。 |

## P1.8b 执行边界

`tools/test_shader_corpus_path_a.sh` 分两层运行：

1. **确定性控制样本**：必须通过。若这些样本失败，说明 Path A 基础能力回退，脚本直接失败。
2. **本地真实/近真实语料扫描**：发现失败时记录为“语料发现问题”，不中断脚本。P1.8b 的目标是扩大暴露面，不是一次性修完所有语料。

这能避免两个问题：

- 真实语料里的坏样本、未初始化变量、语义不匹配不会掩盖已经可用的 Path A 主线。
- 后续 P4/P6 可以把“语料发现问题”逐步收敛成最小复现、回归测试和修复任务。

## 后续回归测试建议

| 层级 | 建议样本 | 目标 |
|------|----------|------|
| 单元测试 | 内嵌 Slang 控制样本 + 最小失败样本 | 锁定资源声明、阶段 profile、语义映射。 |
| 集成测试 | P1.6/P1.7/P1.8/P1.8b 脚本 | 验证 Slang→DXIL→MSC→metallib 全链路。 |
| 兼容性测试 | 合法来源的游戏着色器最小化样本 | 验证真实游戏特征不会回归。 |
| 性能测试 | P1.9 基准脚本读取同一语料目录 | 测量编译耗时、缓存收益和失败分布。 |

