# P3.1a — libmetal_bridge 模块骨架与 C ABI 收口

> 目标：在开始 `MetalNative.cs` 和 C++ 实现前，先固定桥接边界，减少后续返工。

## 一、任务结论

`libmetal_bridge` 的 Phase 3 边界先固定为：

- **C ABI 单入口头文件**：`src/libmetal_bridge/include/metal_bridge.h`
- **opaque handle 设计**：C# 侧统一以 `nint/IntPtr` 持有，不泄露 metal-cpp/ObjC 类型
- **模块最小集合**：`device / queue / buffer / texture / compiler / command_mapper / presenter`
- **统一生命周期**：所有 handle 共用 `metal_release`
- **统一错误读取**：先保留 `metal_get_last_error_message()`，线程模型后续再细化

## 二、为什么现在就要收口

如果不先做这一步，后续会出现两类高概率返工：

1. `MetalNative.cs` 先写一套 P/Invoke，C++ 真正实现时再改函数签名
2. `MetalDevice.cs`、`MetalBuffer.cs`、`MetalShaderCompiler.cs` 先按面向对象直觉设计，最后发现 C ABI 不适合映射

所以 `P3.1a` 的重点不是“实现功能”，而是把**边界先钉住**。

## 三、模块边界

当前收口的模块职责如下：

- `MetalDevice.cpp`
  负责 `MTLDevice` 创建、GPU 选择、能力查询
- `MetalQueue.cpp`
  负责 `MTLCommandQueue`，后续吸收 command buffer 提交入口
- `MetalBuffer.cpp`
  负责 `MTLBuffer` 创建、映射、上传、读回
- `MetalTexture.cpp`
  负责 `MTLTexture` 创建、格式映射、上传下载
- `ShaderCompiler.cpp`
  负责 Slang + MSC + 缓存；`P3.1b` 再收口单例和 workaround
- `CommandMapper.cpp`
  负责 GAL/Maxwell 状态翻译，不持有设备资源所有权
- `Presenter.cpp`
  负责 `CAMetalLayer`、交换链和显示路径

## 四、C ABI 规则

### 1. 只暴露 opaque handle

头文件中统一使用：

- `typedef struct metal_device metal_device;`
- `typedef struct metal_queue metal_queue;`
- `typedef struct metal_buffer metal_buffer;`
- `typedef struct metal_texture metal_texture;`
- `typedef struct metal_shader_compiler metal_shader_compiler;`

原因：

- C# P/Invoke 好映射
- 不把 metal-cpp 类型带过边界
- 允许实现层未来从纯 C++ 扩到 `.mm`/桥接实现，而不破坏 C# 接口

### 2. 生命周期统一

先只固定一个统一释放入口：

- `metal_release(void* handle)`

这样可以避免 Phase 3 就为每种对象发明一套 `destroy_xxx()` API。

### 3. 先收口最小稳定面

当前固定的基础入口只有：

- `metal_bridge_abi_version()`
- `metal_release()`
- `metal_get_last_error_message()`
- `metal_create_device()`
- `metal_get_device_info()`
- `metal_create_queue()`
- `metal_acquire_shader_compiler()`

剩余函数全部延后到 Phase 4 按模块补齐。

## 五、对后续任务的影响

### 直接服务的任务

- `P3.4`：`MetalNative.cs`
- `P3.7`：`MetalDevice.cs`
- `P3.8`：`MetalShaderCompiler.cs`
- `P3.9`：`MetalBuffer/Texture/Sampler stubs`

### 配套前置

- `P3.1b` 将继续收口：
  - 编译器单例
  - workaround 位掩码
  - language version 3.2 等策略

## 六、当前不做的事情

为避免过早定死实现，`P3.1a` 明确不做：

- 不定义完整 buffer/texture/pipeline descriptor
- 不定义全部渲染/计算/同步函数
- 不决定 `render encoder / compute encoder / blit encoder` 是否长期暴露给 C#
- 不提前把 `MetalProgram`、`MetalPipeline` 的所有调用面写死

这些都留到 `P3.1b`、`P4.1`、`P4.2`、`P4.3` 结合真实实现再展开。

## 七、产物

- `src/libmetal_bridge/include/metal_bridge.h`
- `src/libmetal_bridge/CMakeLists.txt`
- `src/libmetal_bridge/README.md`
- 本文档
