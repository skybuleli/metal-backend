# Phase 2 会话日志

## 阶段摘要

- **阶段**: Phase 2 — 渐进式渲染 Demo
- **当前状态**: 已完成
- **已完成任务**: P2 规划收口、P2.0–P2.12
- **关键结果**:
  - D1–D8 Demo 均完成构建、运行与证据固化
  - Path A 已进入 D2、D6、D7、D8 等关键 Demo 链路
  - D8 在 M1 上完成复杂离屏演示，性能显著超过阶段目标

---

## 2026-06-12 | P2 任务扩充与 Demo 验收标准收口 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 将 P2 从 6 个粗任务扩展为 D1-D8 渐进式验证链
- **变更**:
  - 更新 `PROGRESS.md`：P2 增加 `P2.0` 到 `P2.12`，覆盖 D1-D8 构建、运行、证据和 D8 性能验证
  - 更新 `src/demos/README.md`：补充 D1-D8 路线图、任务映射、证据要求和约束
  - 重新生成 `NEXT_TASK.md`：下一任务切换为 `P2.0`
  - 将 Phase 1 阶段状态标记为完成，避免阶段状态与 P1 全部完成事实冲突
- **验证**:
  - `python3 tools/gen_next_task.py`
  - `python3 tools/verify_progress.py` → 25/132 任务完成，验证通过
- **说明**:
  - Rust FFI 两项从 P2 移出；当前蓝图主线是 C++/C# 混合，P2 应聚焦 Metal Demo 能力验证

---

## 2026-06-12 | P2.0 Demo 规格、构建入口与证据格式收口 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ P2.0 完成，Phase 2 进入 D1 实现前的可执行状态
- **变更**:
  - 修复顶层 `Makefile`，使 `make help`、`make build-demos`、`make clean` 指向当前 `src/` 目录结构
  - 重写 `src/demos/Makefile`，提供 `build-demos`、`d1`、`run-d1` 等稳定命名入口
  - 新增 `docs/p2-demo-evidence.md`，统一 D1-D8 的构建日志、运行证据、性能 JSON 规范
  - 更新 `README.md` 与 `src/demos/README.md`，同步目录结构、P2 证据规范和当前阶段约束
  - 新增 `docs/evidence/P2.0-build-entry.txt` 作为构建入口验证证据
- **验证**:
  - `make help`
  - `make build-demos`
  - `make -C src/demos d1`
  - `python3 tools/verify_progress.py`

---

## 2026-06-12 | P2.1 D1 Hello Triangle 离屏渲染落地 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ `D1` 已实现并可在 Apple M1 上离屏渲染彩色三角形
- **变更**:
  - 新增 `src/demos/d1/src/main.cpp`
  - 新增 `src/demos/d1/Makefile`
  - 更新 `src/demos/d1/README.md`
  - 更新 `src/demos/Makefile`
  - 新增 `docs/evidence/P2.1-build.txt`
- **验证**:
  - `make -C src/demos d1`
  - `make -C src/demos run-d1`
  - `file src/demos/d1/out/triangle.ppm`
  - `wc -c src/demos/d1/out/triangle.ppm`

---

## 2026-06-12 | P2.2 D1 运行验证与 PNG 证据产物 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ D1 已补齐人眼可读的 PNG 证据和运行说明
- **变更**:
  - 更新 `src/demos/d1/Makefile`
  - 更新 `src/demos/d1/README.md`
  - 新增 `docs/evidence/P2.2-d1-triangle.ppm`
  - 新增 `docs/evidence/P2.2-d1-triangle.png`
  - 新增 `docs/evidence/P2.2-run.txt`
  - 新增 `docs/evidence/P2.2-meta.json`
- **验证**:
  - `make -C src/demos/d1 evidence`
  - 人工查看 `docs/evidence/P2.2-d1-triangle.png`

---

## 2026-06-12 | P2.3 D2 Textured Quad 接入 Path A 与纹理采样 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ `D2` 已通过 `Slang → DXIL → MSC → metallib` 渲染离屏纹理 quad
- **变更**:
  - 新增 `src/demos/d2/shaders/quad.slang`
  - 新增 `src/demos/d2/src/main.cpp`
  - 新增 `src/demos/d2/Makefile`
  - 更新 `src/demos/d2/README.md`
  - 更新 `src/demos/Makefile`
  - 新增 `docs/evidence/P2.3-build.txt`
- **验证**:
  - `make -C src/demos d2`
  - `make -C src/demos run-d2`
  - `make build-demos`

---

## 2026-06-12 | P2.4 D2 运行验证与 MSC 参数绑定修复 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ D2 已生成可人工查看的 PNG 截图与真实运行日志，纹理 quad 可见
- **变更**:
  - 更新 `src/demos/d2/src/main.cpp`
  - 更新 `src/demos/d2/Makefile`
  - 更新 `src/demos/d2/README.md`
  - 新增 `docs/evidence/P2.4-d2-textured-quad.ppm/png`
  - 新增 `docs/evidence/P2.4-run.txt`
  - 新增 `docs/evidence/P2.4-meta.json`
  - 新增 `docs/evidence/P2.4-vertex-reflection.json`
  - 新增 `docs/evidence/P2.4-fragment-reflection.json`
- **关键发现**:
  - MSC 片段着色器要求通过顶层参数缓冲传入纹理与采样器描述

---

## 2026-06-12 | P2.5 D3 Multi-Texture 与多采样器状态 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ D3 已通过 Path A 渲染双纹理混合结果，并固化 PNG、运行日志和反射文件
- **变更**:
  - 新增 `src/demos/d3/shaders/multi_texture.slang`
  - 新增 `src/demos/d3/src/main.cpp`
  - 新增 `src/demos/d3/Makefile`
  - 更新 `src/demos/d3/README.md`、`src/demos/README.md`、`src/demos/Makefile`
  - 新增 `docs/evidence/P2.5-*`
- **关键发现**:
  - D3 的 MSC 顶层参数缓冲布局为“全部 SRV，随后全部 Sampler”

---

## 2026-06-12 | P2.6 D4 重新实现（手写 MSL）+ 收尾 cleanup | ✅ 完成

- **Agent**: Reasonix
- **结果**: ✅ D4 已正确实现并固化证据
- **实现变更**:
  - 手写 MSL 内嵌 main.cpp
  - 补齐 Uniform Buffer、3D 变换、深度测试、背面剔除、Phong 光照
- **审查补记**:
  - 后续审查曾指出某一版实现未满足 D4 要求，并已回退状态、修正虚假证据
  - 经验教训是 DXIL 限制不能成为跳过核心需求的理由

---

## 2026-06-12 | P2 阶段补充轻量窗口预览任务规划 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 已把 `P2.6a` 正式加入 Phase 2 规划

---

## 2026-06-12 | P2.6a 轻量预览窗口落地 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 为 `D1-D4` 增加本地可视预览窗口，保留原有离屏证据链

---

## 2026-06-12 | P2.6b D4 实时旋转窗口版 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 为 `D4` 增加可实时观看的自动旋转窗口版

---

## 2026-06-12 | P2.7 D5 高级贴图子集 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 已交付 `法线贴图 + Skybox/Cubemap + 4x MSAA` 的 D5 第一版

---

## 2026-06-12 | P2.7a D5 展示增强与 MSAA 对比证据 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 将 D5 从“功能已通”推进到“画面会说话”的展示版

---

## 2026-06-12 | P2.8 D6 高级光照与后处理链路 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 已交付 `Shadow Map + HDR + Tone Mapping + Bloom` 的 D6 离屏 Demo

---

## 2026-06-12 | P2.8a/P2.8b/P2.8c D6 Path A 桥接、双路径对照与语义回归 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 已将 D6 主场景 pass 切到 `Slang -> DXIL -> MSC -> metallib`，并补齐双路径对照与高风险语义回归证据
- **关键结果**:
  - 双路径对照仅有 `10 / 589824` 个像素不一致，`RMSE = 0.035754`

---

## 2026-06-12 | P2.9 D7 GPU-Driven 粒子与间接绘制链路 | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ 已交付 `Path A compute 粒子更新 + GPU 写 indirect 参数 + instanced quad 渲染` 的 D7 离屏 Demo

---

## 2026-06-13 | P2.10 D8 Complex Showcase | ✅ 完成

- **Agent**: Codex
- **结果**: ✅ D8 综合演示完整交付，M1 上达到 812 FPS
