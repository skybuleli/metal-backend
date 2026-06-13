# P3.1b — MetalShaderCompiler 单例与 workaround 位掩码设计

> 目标：在 `P3.8` 和 `P4.2` 之前，先固定 `ShaderCompiler` 的生命周期策略、配置入口和已知 workaround 集合。

## 一、任务结论

`MetalShaderCompiler` 先按以下原则收口：

- **默认单例复用**：以 `metal_device` 为粒度复用一个编译器实例
- **配置可见**：通过 `metal_shader_compiler_config` 传递 workaround 与语言版本
- **workaround 可枚举**：每个已知 workaround 在 C ABI 中有稳定 bit 位
- **默认保守**：默认启用 compiler singleton 与 language version 3.2
- **后续渐进实现**：Phase 4.2 再补具体 `Slang → DXIL → MSC → metallib` 过程

## 二、为什么要单例

根据 kk 报告：

- 多个 `MTL4Compiler` 实例并发使用会导致崩溃
- 推荐做法是“全局表 + device 维度复用 + 引用计数”

因此本项目默认策略为：

1. `metal_device` 创建后，对应一个编译器实例槽位
2. `metal_acquire_shader_compiler(device, &compiler)` 默认返回该 device 的共享实例
3. `metal_release()` 负责递减引用计数
4. 只有显式关闭 `METAL_WA_COMPILER_SINGLETON` 时，才允许退化到“每次新建”

## 三、为什么要位掩码

workaround 会越来越多，如果散落在代码里，后面会出现：

- 日志很难说明当前到底启用了哪些修复
- 回归时无法稳定复现某个兼容问题
- `MetalNative.cs` 和 C++ 两侧很容易对开关名字理解不一致

所以先在 C ABI 层固定 bit 位。

## 四、当前固定的 workaround 集合

### 直接来自 kk 报告且与本项目直接相关

- `METAL_WA_COMPILER_SINGLETON`
  默认开启。解决 MTL4Compiler 多实例风险。

- `METAL_WA_LANG_VERSION_3_2`
  默认开启。原因是 `MTLLanguageVersion4_0` 在部分 float16 VS 场景可能超时。

- `METAL_WA_DISCARD_GUARD`
  先占位。后续用于 Metal `discard` 语义兼容。

- `METAL_WA_HELPER_INVOCATION`
  先占位。后续用于 helper invocation 降级。

- `METAL_WA_SAMPLE_MASK`
  先占位。后续用于 MSAA/sample mask 兼容。

### 当前项目前瞻性保留

- `METAL_WA_TESS_TO_COMPUTE`
  为 Phase 5 的 tessellation fallback 预留。

- `METAL_WA_TEXTURE_FORMAT`
  为格式降级或特殊纹理兼容预留。

## 五、默认配置

当前默认配置建议为：

```c
enabled_workarounds =
    METAL_WA_COMPILER_SINGLETON |
    METAL_WA_LANG_VERSION_3_2;

disabled_workarounds = 0;
metal_language_version = 0; // 0 表示“按 workaround 决定默认值”
```

解释：

- 先只默认打开最有证据支撑的两个开关
- 其他 workaround 保留命名，但不在 Phase 3 提前强行启用
- 具体 `MTLLanguageVersion3_2` 常量映射放到实现层，不在 C ABI 暴露 Apple 枚举

## 六、C ABI 新增接口

为支撑上面的设计，`metal_bridge.h` 新增：

- `metal_shader_compiler_config`
- `metal_get_default_shader_compiler_config()`
- `metal_configure_shader_compiler()`
- `metal_shader_compiler_get_workarounds()`

这些接口的作用是：

- 让 `MetalNative.cs` 能拿到默认配置
- 让后续测试和调试能够改 workaround 开关
- 不把具体编译过程 API 和配置 API 混在一起

## 七、对后续任务的直接影响

- `P3.4 MetalNative.cs`
  需要按当前配置结构体与函数声明 P/Invoke

- `P3.8 MetalShaderCompiler.cs`
  需要将单例/配置/workaround 作为托管层职责之一

- `P4.2.0 MSC/Metal 限制验证矩阵`
  将以这些 workaround 名称作为测试分类标签

- `P6.2.0 回归样本`
  将以这些 workaround 位作为实验开关

## 八、当前明确不做的事情

`P3.1b` 不做以下内容：

- 不实现真实的编译器全局哈希表
- 不实现环境变量解析
- 不决定缓存目录结构
- 不实现 Slang API / MSC 调用
- 不把所有编译选项提前细化到 descriptor 级别

这些留到 `P3.8` 和 `P4.2.x`。

## 九、产物

- `src/libmetal_bridge/include/metal_bridge.h`
- `src/libmetal_bridge/src/ShaderCompiler.cpp`
- 本文档
