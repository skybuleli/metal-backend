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

头文件中统一使用（当前 16 个 handle 类型）：

- `typedef struct metal_device metal_device;`
- `typedef struct metal_queue metal_queue;`
- `typedef struct metal_buffer metal_buffer;`
- `typedef struct metal_texture metal_texture;`
- `typedef struct metal_sampler metal_sampler;`
- `typedef struct metal_library metal_library;`
- `typedef struct metal_shader_compiler metal_shader_compiler;`
- `typedef struct metal_render_pipeline metal_render_pipeline;`
- `typedef struct metal_compute_pipeline metal_compute_pipeline;`
- `typedef struct metal_command_buffer metal_command_buffer;`
- `typedef struct metal_render_encoder metal_render_encoder;`
- `typedef struct metal_compute_encoder metal_compute_encoder;`
- `typedef struct metal_blit_encoder metal_blit_encoder;`
- `typedef struct metal_presenter metal_presenter;`
- `typedef struct metal_fence metal_fence;`
- `typedef struct metal_shared_event metal_shared_event;`

原因：

- C# P/Invoke 好映射
- 不把 metal-cpp 类型带过边界
- 允许实现层未来从纯 C++ 扩到 `.mm`/桥接实现，而不破坏 C# 接口

### 2. 生命周期统一

先只固定一个统一释放入口：

- `metal_release(void* handle)`

这样可以避免 Phase 3 就为每种对象发明一套 `destroy_xxx()` API。

### 3. 当前已实现的基础入口

| 模块 | 入口函数 | 实现阶段 |
|:-----|:---------|:--------:|
| 基础 | `metal_bridge_abi_version()` / `metal_release()` / `metal_get_last_error_message()` | P3.1a |
| 设备 | `metal_create_device()` / `metal_get_device_info()` / `metal_get_device_caps()` | P4.1.1 |
| 队列 | `metal_create_queue()` | P4.1.1 |
| 编译器 | `metal_acquire_shader_compiler()` / `metal_get_default_shader_compiler_config()` / `metal_configure_shader_compiler()` / `metal_shader_compiler_get_workarounds()` | P3.1b |
| 缓冲区 | `metal_create_buffer()` / `metal_create_buffer_with_bytes()` / `metal_create_buffer_from_pointer()` / `metal_buffer_get_info()` / `metal_map_buffer()` / `metal_unmap_buffer()` / `metal_flush_buffer()` / `metal_buffer_get_cpu_address()` | P4.1.2 |
| 纹理 | `metal_create_texture()` / `metal_texture_get_info()` / `metal_texture_upload()` / `metal_texture_readback()` / `metal_pixel_format_get_info()` | P4.1.3 |

仍延后到后续 Phase 完成的函数参见 §六。

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

## 六、当前不做的事情（已做 vs 仍延后）

> ⚠️ 本文档最初写于 P3.1a。以下列表已根据 P4.1.2/P4.1.3 的完成情况更新。

### ✅ 已完成的

- **Buffer 完整 C ABI**（`create / create_with_bytes / create_from_pointer / get_info / map / unmap / flush / get_cpu_address`）— P4.1.2
- **Texture 完整 C ABI**（`create / get_info / upload / readback / pixel_format_get_info` + `metal_pixel_format` + `metal_texture_type` + `metal_texture_usage` 枚举）— P4.1.3
- **硬件限制常量**（`metal_limits.h`，9 类 30+ 常量 + 存储模式策略 + 对齐辅助宏）— P4.1.0
- **内部结构体共享**（`metal_internal.h`，device/queue/buffer/texture 的 struct 定义）— P4.1.2
- **Presenter 完整 C ABI**（`metal_create_presenter / metal_presenter_get_info / metal_presenter_resize / metal_presenter_present_texture`）— P4.4.3

### ⏸️ 仍延后的

| 领域 | 延后到 | 原因 |
|:-----|:------|:-----|
| `metal_create_sampler` | P4.1.4 | 采样器状态映射，无阻塞依赖 |
| `metal_create_render_pipeline` / `metal_create_compute_pipeline` | P4.3.1 | 需要等动态函数签名基础设施 |
| `metal_begin_command_buffer` / `metal_commit_command_buffer` / `metal_wait_command_buffer` | P4.4.1 | 需要等 command submission 基础设施 |
| `metal_encoder_*` 系列 | P4.3.x | 所有 setBuffer/setTexture/draw 入口 |
| `metal_fence` / `metal_shared_event` | P4.4.x | 同步原语 |
| true pipeline descriptor | P4.3.x | 动态函数签名与管线反射 |
| `render encoder / compute encoder / blit encoder` 是否暴露给 C# | P4.3.x | 设计决策待定 |

## 七、产物

- `src/libmetal_bridge/include/metal_bridge.h`
- `src/libmetal_bridge/CMakeLists.txt`
- `src/libmetal_bridge/README.md`
- 本文档
