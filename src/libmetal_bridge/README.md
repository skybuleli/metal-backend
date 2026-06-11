# libmetal_bridge — C++ Metal 运行时桥接层

> Phase 4 实现。本模块是 C++ 编写的 Metal 运行时，通过 C ABI 暴露给 C# 调用。

## 模块结构（规划）

```
libmetal_bridge/
├── CMakeLists.txt          # CMake 构建配置
├── include/
│   └── metal_bridge.h      # C ABI 头文件（P/Invoke 接口）
├── src/
│   ├── MetalDevice.cpp     # MTLDevice 创建、GPU 选择
│   ├── MetalQueue.cpp      # MTLCommandQueue 管理
│   ├── MetalBuffer.cpp     # MTLBuffer（Managed/Private/Shared 存储模式）
│   ├── MetalTexture.cpp    # MTLTexture + Maxwell→MTLPixelFormat 映射
│   ├── ShaderCompiler.cpp  # Slang API + libmetalirconverter 调用 + 缓存
│   ├── CommandMapper.cpp   # Maxwell 状态 → Metal API 翻译
│   └── Presenter.cpp       # CAMetalLayer、交换链管理
└── tests/
    └── test_device.cpp     # 设备创建单元测试
```

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
