# 贡献指南

## 分支策略
main（稳定） ← develop ← feature/* | hotfix/* | release/*

## 提交规范（约定式提交）
```
<type>(<scope>): <描述>
type:  feat|fix|docs|test|refactor|perf|build|ci|chore
scope: metal|gal|shader|demo|cmd|tools|docs|test
```

## Agent 会话流程
1. 读取 NEXT_TASK.md
2. 在 PROGRESS.md 中标记 🔄
3. 执行 → 验证 → 标记 ✅
4. 提交 + 运行 gen_next_task.py
5. 记录到 SESSION_LOG.md

## 语言要求
本项目所有文档、注释、提交信息均使用简体中文。代码标识符（变量名、函数名、类名）可使用英文，但注释必须使用中文。AI Agent 在进行推理和思考时也应使用中文上下文。
