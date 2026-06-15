# Code-Review-Agent-for-Ascend-Operators

> **GitHub**: https://github.com/Westreety/Code-Review-Agent-for-Ascend-Operators

We are designing a Code Review Agent project for AscendC Huawei Ascend operators. The goal is to detect potential bugs in operators developed by third-party developers, with plans to integrate it into the complete engineering workflow in the future.

我们目前正在完成一个面向华为昇腾（AscendC）算子的代码审查智能体（Code Review Agent）项目。该项目旨在发现第三方开发者所编写算子中的潜在缺陷（Bug），并计划在未来将其集成至完整的工程研发工作流中。

## 项目概述

本项目基于华为昇腾 CANN 平台的算子（目前以Mul算子为例），构建了一套完整的**算子错误分类体系**和**Agent 代码审查评测框架**。通过系统性地注入不同类型错误，评估 AI Code Review Agent 的 bug 发现能力；同时使用无注入的原始代码，验证 Agent 能否发现源码中固有的缺陷。

**核心目标**：量化评估 AI Agent 在 NPU 算子代码审查中的能力边界。

## 错误分类体系

错误分类体系位于 `error_taxonomy/` 目录：

- **v1.0**：8 大类 35 种，覆盖 op_api → op_host → op_kernel 三层架构
- **v2.0**：9 大类 43 种，新增分布式通信类、Cube/Vector 精度截断、L1/UB 超限、死锁、Padding 污染等

## 目录结构

```
.
├── PROJECT_SUMMARY.md                  # 项目工作总结
├── error_taxonomy/                     # 错误分类体系
│   ├── ERROR_TAXONOMY_v1.0.md          # v1.0：8大类35种（op_api/op_host/op_kernel）
│   └── ERROR_TAXONOMY_v2.0.md          # v2.0：9大类43种（新增分布式通信等）
├── agent_arena/                        # Agent 评测主目录
│   ├── eval_prompt.md                  # 评测提示词
│   ├── cases/                          # 评测用例
│   ├── examples/                       # 测试模板
│   └── output/                         # Agent 输出结果
├── custom_op/                          # 自定义算子（Heisenbug 探索）
├── error_testset/                      # 错误测试集原始文档
├── Mul算子/                            # 算子架构分析文档
└── deploy.sh                           # 部署脚本
```

## 快速开始

详见 `PROJECT_SUMMARY.md`。

## TODO

- [ ] 更多错误类型的注入（完善整个Pipeline）
- [ ] 将评测框架扩展到其他 CANN 算子
- [ ] 完善错误体系
