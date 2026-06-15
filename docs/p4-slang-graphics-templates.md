# P4.6.2 Slang 原生图形着色器模板集

## 目标

为后续 `CommandMapper`、手写 2D 最小样本和真实游戏排查提供一组统一的 Slang 原生图形模板，避免继续把 GLSL 习惯直接带进 Path A。

这些模板全部遵循当前主路径：

`Slang 原生语法 → DXIL → MSC → metallib`

## 模板列表

### 1. `fullscreen_quad.slang`

路径：`src/shader_templates/slang_graphics/fullscreen_quad.slang`

用途：

- 最小屏幕空间采样模板
- 验证 `SV_VertexID` 路线
- 验证 `SV_Position + TEXCOORD0` 的最小 varying 组合

关键点：

- 顶点阶段不依赖显式顶点缓冲
- 片段阶段只依赖 `Texture2D + SamplerState`
- 适合做 blit、present 前后拷贝和后处理 smoke test

### 2. `sprite_alpha.slang`

路径：`src/shader_templates/slang_graphics/sprite_alpha.slang`

用途：

- 2D sprite / atlas / HUD 的主模板
- 验证 `ConstantBuffer<T>`、顶点色、纹理采样和 alpha discard

关键点：

- 顶点阶段使用 `ConstantBuffer<SpriteSceneData>`
- `POSITION / TEXCOORD0 / COLOR0` 明确落到 `SV_Position / TEXCOORD0 / COLOR0`
- 片段阶段做 `texture * vertexColor * globalTint`
- 用最小 `discard` 表达透明裁剪路径

### 3. `tilemap_camera.slang`

路径：`src/shader_templates/slang_graphics/tilemap_camera.slang`

用途：

- tilemap、相机滚动和 atlas 取样的基线模板
- 为后续 `OpenSupaplex` / `NXEngine-evo` 一类样本提供对照

关键点：

- 顶点阶段使用 `ConstantBuffer<TilemapSceneData>`
- `scrollPixels` 在 VS 中处理，避免 FS 重复做屏幕空间运算
- `tileIndex + tileUv → atlasUv` 的组合逻辑显式可见
- varying 保持为 `TEXCOORD0 + COLOR0`，避免无意义扩散

## 模板约束

所有新图形模板默认遵守以下规则：

1. 图形阶段统一使用 `vertex` / `fragment` 入口，不用 GLSL 风格 `main()` 推断。
2. 顶点输出必须显式包含 `SV_Position`。
3. 插值数据优先用 `TEXCOORDN` / `COLORN`，不要依赖 GLSL 风格 location 习惯。
4. uniform 数据统一走 `ConstantBuffer<T>`，不再使用 GLSL `layout(std140)` 块语法。
5. 纹理采样统一使用 `Texture2D<T> + SamplerState`。
6. 片段阶段 profile 固定按 `ps_6_0` 验证，避免 `sm_6_0` 无 DXIL 产物的问题。

## 对 CommandMapper 的直接要求

后续 `CommandMapper` 生成 Slang 原生语法时，至少应优先贴近以下形态：

- 顶点输入：`POSITION / NORMAL / TEXCOORDN / COLORN`
- 顶点输出：`SV_Position + TEXCOORDN/COLORN`
- uniform：`ConstantBuffer<T>`
- 采样：`Texture2D<T>.Sample(SamplerState, uv)`
- 透明裁剪：必要时允许最小 `discard`

如果生成结果偏离这组模板太远，优先怀疑是“路线设计不一致”，而不是先怀疑 MSC 或 Metal。
