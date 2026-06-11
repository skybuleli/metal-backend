# 渐进式渲染 Demo（D1–D8）

> Phase 2 实现。8 个渐进复杂度 Demo，从三角形到完整 3D 场景。

## Demo 路线图

| 级别 | 名称 | 新增内容 | 出口标准 |
|------|------|----------|----------|
| D1 | 三角形 | MTLDevice + 硬编码着色器 | 彩色三角形渲染 |
| D2 | 顶点缓冲 | MTLBuffer + 顶点数据 | 正方形渲染 |
| D3 | 统一缓冲 | UBO + 旋转动画 | 旋转三角形 |
| D4 | 纹理 | MTLTexture + 采样器 | 纹理贴图正方形 |
| D5 | 深度测试 | DepthStencilState | 3D 立方体 |
| D6 | Path A 着色器 | Slang→DXIL→MSC→metallib | metallib 加载渲染 |
| D7 | 多通道 | 离屏渲染 + 后处理 | Bloom 效果 |
| D8 | 复杂场景 | 所有 D1-D7 组合 + Phong 光照 | ≥60fps |

## 技术栈

- **语言**: C++17 + metal-cpp
- **着色器**: 硬编码 MSL（D1–D5），Path A metallib（D6–D8）
- **构建**: CMake 或独立 Makefile

## 构建

```bash
make all     # 编译所有 Demo
make d1      # 仅编译 D1
make run-d1  # 运行 D1
```

## 依赖

- Xcode Command Line Tools（含 Metal 框架）
- metal-cpp 头文件
- metal-shaderconverter 4.0（D6–D8）

## 参考

- [Metal Sample Code](https://developer.apple.com/metal/sample-code/)
- `docs/metal-api.md` — metal-cpp 核心模式速查
