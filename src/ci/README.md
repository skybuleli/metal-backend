# CI/CD 流水线配置

## 流水线文件

| 文件 | 用途 |
|------|------|
| `ci.yml` | 主 CI：构建 + 验证 + 测试 |
| （更多待 Phase 6 添加） | |

## 触发条件

- `push` 到 `main` 或 `develop`
- `pull_request` 到 `main`

## 作业

1. **环境验证** — 运行 `tools-verify.sh`
2. **构建 libmetal_bridge** — CMake 编译 C++ 桥接层
3. **构建 Demo** — 编译 D1–D8 渲染 Demo
4. **代码检查** — Python 编译检查 + PROGRESS.md 验证 + ShellCheck

## 运行器要求

- macOS（Apple Silicon 或 Intel + Metal 支持）
- Xcode Command Line Tools（含 Metal 框架）
- Homebrew（安装 SPIRV-Tools 等依赖）
