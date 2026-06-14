# libmetal_bridge — C++ Metal 运行时桥接层

> `P3.1a` 已先收口模块骨架与 C ABI 边界。Phase 4 在此基础上逐步补实现。

## 模块结构（已收口）

```text
libmetal_bridge/
├── CMakeLists.txt                    # CMake 构建配置
├── include/
│   ├── metal_bridge.h                # C ABI 头文件（opaque handle + 全部入口）
│   ├── metal_limits.h                # Metal 硬件限制常量与资源对齐策略
│   └── metal_internal.h              # 内部结构体定义（device/queue/buffer/texture）
├── src/
│   ├── MetalDevice.cpp               # MTLDevice 创建、GPU 选择、能力查询
│   ├── MetalQueue.cpp                # MTLCommandQueue + command buffer 入口
│   ├── MetalBuffer.cpp               # MTLBuffer / create / map / unmap / flush
│   ├── MetalTexture.cpp              # MTLTexture + 格式映射 + 上传/回读
│   ├── ShaderCompiler.cpp            # Slang + MSC + 缓存 + workaround（P4.2 实现）
│   ├── CommandMapper.cpp             # GAL/Maxwell 状态 → Metal API 翻译（P5 实现）
│   └── Presenter.cpp                 # CAMetalLayer、drawable 交换链、blit present（P4.4.3）
└── tests/
    ├── test_device.cpp               # 11 个设备与生命周期测试
    ├── test_buffer.cpp               # 19 个缓冲区测试
    └── test_texture.cpp              # 19 个纹理测试（含格式映射 + 上传/回读）
```

## C ABI 设计原则

- **单一边界**：C# 不直接触碰 metal-cpp/ObjC 类型，统一通过 `metal_bridge.h`
- **opaque handle**：所有对象在 C ABI 中只暴露不透明指针，生命周期统一交给 `metal_release`
- **最小稳定面**：Phase 3 只固定句柄类型、错误码、版本和少量入口，不提前发明大批函数
- **按模块扩展**：Phase 4 再按 `device / queue / buffer / texture / compiler / presenter` 逐组补接口

## 当前确定的 opaque handle 集合（16 个）

`metal_device` / `metal_queue` / `metal_buffer` / `metal_texture` / `metal_sampler` / `metal_library` / `metal_shader_compiler` / `metal_render_pipeline` / `metal_compute_pipeline` / `metal_command_buffer` / `metal_render_encoder` / `metal_compute_encoder` / `metal_blit_encoder` / `metal_presenter` / `metal_fence` / `metal_shared_event`

> 定义在 `metal_bridge.h` 的 `metal_handle_type` 枚举中。

## 模块拆分约束

- `MetalDevice` 持有 `MTLDevice` 与能力查询
- `MetalQueue` 持有 `MTLCommandQueue`，后续吸收 command buffer 提交入口
- `ShaderCompiler` 独立负责 Slang + MSC，并在 `P3.1b` 收口单例与 workaround 设计
- `Presenter` 只处理显示与交换链，不混入资源创建逻辑；当前实现通过 `CAMetalLayer` 的 drawable + blit 提交完成 present
- `CommandMapper` 只做状态翻译，不承担设备/资源所有权

## 与 Phase 3 任务的关系

- `P3.1a`：确定模块边界与 C ABI/opaque handle 方案
- `P3.1b`：确定 `MetalShaderCompiler` 单例、默认配置与 workaround 位掩码
- `P3.4`：`MetalNative.cs` 按本头文件声明 P/Invoke
- `P3.7-P3.9`：再逐步补 C# 侧包装类型与 stub

## ShaderCompiler 约束（P3.1b）

- 默认按 `metal_device` 维度复用编译器实例
- 默认启用：
  - `METAL_WA_COMPILER_SINGLETON`
  - `METAL_WA_LANG_VERSION_3_2`
- 其他 workaround 先固定命名与 bit 位，具体行为在 Phase 4.2 再实现
- 编译器配置通过 `metal_shader_compiler_config` 传递，不在 Phase 3 暴露 Apple 私有枚举

## 技术栈

- **语言**: C++17
- **Metal 绑定**: [metal-cpp](https://developer.apple.com/metal/cpp/)（非 ObjC/Swift）
- **编译**: CMake + Apple Clang（CLT SDK）
- **着色器编译**: Slang API + libmetalirconverter（MSC 运行时库）

## 关键设计决策

- **ADR-001**: 使用 metal-cpp 而非 Swift/ObjC，便于与 Rust/C# FFI 交互
- **ADR-003**: C++/C# 混合架构，C ABI 为桥接边界
- **存储模式**: M1 是 UMA 架构，Shared 和 Managed 模式行为相同

## 构建

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## 依赖

- Xcode Command Line Tools（含 macOS SDK 和 Metal 框架）
- metal-cpp 头文件（从 Apple 下载）
- libmetalirconverter.dylib（随 metal-shaderconverter 4.0 安装）

## 参考

- [Apple Metal-CPP 文档](https://developer.apple.com/metal/cpp/)
- [dxmt 实现参考](https://github.com/3Shain/dxmt) — 3Shain/dxmt v0.80
