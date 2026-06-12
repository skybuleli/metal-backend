# 渐进式渲染 Demo（D1–D8）

> Phase 2 实现。8 个渐进复杂度 Demo，从三角形到完整 3D 场景。
>
> 目标不是做展示页面，而是建立一条可重复验证的 Metal 能力梯子：每一级只新增一组能力，保留构建日志、运行截图或帧缓冲、必要的 JSON 性能数据，供 P4/P6/P7 继续复用。
>
> 从 `P2.6a` 起，P2 采用双轨验证：
> 1. **离屏证据链**：PPM / PNG / 运行日志，保证可回归、可比对、可自动校验。
> 2. **本地预览窗口**：`NSWindow + CAMetalLayer`，用于人眼调试和日常体验，不替代离屏证据。

## Demo 路线图

| 级别 | 名称 | 新增内容 | 出口标准 |
|------|------|----------|----------|
| D1 | Hello Triangle | metal-cpp、MTLDevice、MTLCommandQueue、手写 MSL、RenderPipelineState | 彩色三角形可渲染，有截图或帧缓冲证据 |
| D2 | Textured Quad | Path A：Slang→DXIL→MSC→metallib，MTLTexture，MTLSampler，UV 坐标 | 纹理 quad 可渲染，有 metallib 加载日志 |
| D3 | Multi-Texture | 多纹理混合、filter/wrap/mipmap 采样模式 | 同一画面可区分多纹理和采样器模式 |
| D4 | Basic Lighting | Uniform Buffer、矩阵变换、深度测试、背面剔除、Phong 光照 | 旋转 3D 物体有稳定深度和光照 |
| D5 | Advanced Texturing | 法线贴图、Cubemap/Skybox、RTT 或 MSAA 子集 | 天空盒和高级贴图路径至少各有一项通过 |
| D6 | Advanced Lighting | Shadow Map、HDR、Tone Mapping、Bloom 后处理 | 阴影和 Bloom 可见，离屏渲染链路可复用 |
| D7 | GPU-Driven | Compute 粒子、Instancing、Indirect Draw | 粒子或实例渲染由 GPU 数据驱动，CPU 每帧提交量可记录 |
| D8 | Complex Showcase | PBR 材质球、阴影、天空盒、后处理、粒子、HUD、自由摄像机 | M1 上 ≥60fps；若未达标，必须产出瓶颈报告 |

## P2 任务映射

| 任务 | 对应 Demo | 交付物 |
|------|-----------|--------|
| P2.0 | 全部 | D1-D8 验收标准、构建入口、证据目录规范 |
| P2.1-P2.2 | D1 | 可构建 Demo、运行截图或帧缓冲、构建日志 |
| P2.3-P2.4 | D2 | Path A metallib、纹理渲染证据、加载日志 |
| P2.5 | D3 | 多纹理采样 Demo 和截图 |
| P2.6 | D4 | 光照与深度 Demo 和截图 |
| P2.6a | D1-D4 预览窗口 | 轻量窗口预览，不替代离屏证据 |
| P2.6b | D4 实时窗口版 | 自动旋转立方体，本地可视验证 3D 光照链路 |
| P2.7 | D5 | 法线贴图 + Skybox/Cubemap + MSAA 子集 Demo 和截图 |
| P2.7a | D5 展示增强 | 更直观的天空盒与法线贴图主图 + 1x/4x MSAA 对比证据 |
| P2.8 | D6 | 阴影/HDR/Bloom Demo 和截图 |
| P2.9 | D7 | GPU 驱动 Demo 和性能记录 |
| P2.10-P2.11 | D8 | 综合场景、FPS/帧时间 JSON、瓶颈报告或通过证据 |
| P2.12 | 全部 | `make build-demos`、回归入口和证据格式 |

## 技术栈

- **语言**: C++17 + metal-cpp
- **着色器**: D1 可使用手写 MSL；D2 起优先验证 Path A metallib；必要时保留手写 MSL 作为隔离对照
- **构建**: 顶层 Makefile 调用 `src/demos/Makefile`，每个 Demo 可有独立构建入口
- **证据**: `docs/evidence/` 下保存构建日志、截图说明、帧缓冲摘要或性能 JSON，字段规范见 `docs/p2-demo-evidence.md`

## 构建

```bash
make build-demos  # 编译所有 Demo
make -C src/demos d1
make -C src/demos run-d1
```

> `P2.0` 结束时，以上命令只要求入口路径正确、目标命名稳定、输出提示清晰；真正的编译逻辑从 `P2.1` 起逐级实现。

## 依赖

- Xcode Command Line Tools（含 Metal 框架）
- metal-cpp 头文件
- metal-shaderconverter 4.0（D2 起的 Path A Demo）

## 约束

- D1-D8 每一级必须独立可运行，不能依赖后一级代码。
- 每一级只能新增本级目标能力；公共工具可以抽到 `src/demos/common/`，但需要保持接口稳定。
- D2 起必须记录着色器来源、成功路径和 metallib 文件大小，避免把 Path A 失败误判成渲染错误。
- 窗口预览属于 P2 的开发体验增强，不等同于 P4 的完整 Presenter / 交换链实现；P2 阶段只要求本地预览能显示、关闭、切换 Demo。
- `P2.6b` 的 D4 实时窗口版仍属于 Demo 级能力验证，不应在此阶段提前抽象成通用 Presenter。
- D8 不达 60fps 时不能简单标记完成，必须给出 CPU/GPU/编译/资源绑定中至少一个明确瓶颈。

## 参考

- [Metal Sample Code](https://developer.apple.com/metal/sample-code/)
- `docs/metal-api.md` — metal-cpp 核心模式速查
