# Agent 评测任务 — Mul 算子 Code Review

## 背景

你是昇腾（Ascend）NPU 的算子代码审查专家。你面前是一份 `aclnnMul` 算子的 **CANN C++ 实现代码**（op_api 层，约 669 行）。

这份代码是一个真实的华为 CANN 开源算子（`ops-math/math/mul/op_api/aclnn_mul.cpp`），运行在 Ascend 910B NPU 上。

## 你的任务

**仔细审查这份 acnn_mul.cpp 代码，找出其中的 bug 或安全隐患。** 具体要求：

1. **定位 bug**：指出具体文件、函数名、行号、bug 类型
2. **写测试代码**：写出一个 C++ 测试程序（参考 CANN examples 风格），用 **精心构造的输入** 触发这个 bug
3. **解释修复方案**：说明为什么这段代码是错的，正确的写法应该是什么
4. **复现并验证**：在 NPU 环境中编译运行你的测试代码，展示 bug 被触发的结果

## NPU 环境信息

- 硬件：Ascend 910B，8 卡
- CANN 版本：8.5.0
- 头文件路径：`/usr/local/Ascend/cann-8.5.0/include`
- 库路径：`/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64`
- 链接库：`-lascendcl -lnnopbase -lcust_opapi`
- 算子库路径：`/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_math/op_api/lib`

## 输出格式

请按以下格式输出：

```
## Bug 分析
- 位置: <文件名>:<函数名>:<行号>
- 类型: <错误分类名称>
- 严重程度: <高/中/低>

## 测试代码
<完整的 C++ 测试代码，可独立编译运行>

## 运行结果
<NPU 上实际运行测试的输出>

## 修复方案
<修复后的代码 diff>
```

## 开始

算子代码在附件 `buggy_aclnn_mul.cpp` 中。请开始你的审查。
