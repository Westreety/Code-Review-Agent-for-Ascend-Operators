# Agent Arena — Mul 算子 Code Review 评测任务

欢迎来到 Agent Arena，你的任务是审查昇腾（Ascend）NPU Mul 算子的 C++ 代码并找出其中隐藏的 bug。

## 目录结构

```
agent_arena/
├── README.md            ← 你正在看的文件
├── eval_prompt.md       ← 详细评测任务说明（必须阅读）
├── examples/            ← 现有测试基建
│   ├── test_aclnn_mul.cpp  ← Mul 算子测试示例
│   └── build.sh            ← 编译脚本
├── cases/
│   ├── case_01/
│   │   └── aclnn_mul.cpp  ← 待审查代码
│   ├── case_02/
│   │   └── aclnn_mul.cpp
│   ├── case_03/
│   │   └── aclnn_mul.cpp
│   └── case_04/
│       └── aclnn_mul.cpp
└── output/
    ├── case_01/           ← 评测报告写在这里
    ├── case_02/
    ├── case_03/
    └── case_04/
```

## 快速开始

1. 阅读 `eval_prompt.md` 了解评测要求
2. 审查 `cases/case_01/aclnn_mul.cpp`
3. 定位 bug → 写测试代码 → 在 NPU 上编译运行 → 输出结论
4. 参考 `examples/` 中的现有测试代码加速编写

## NPU 环境

- 硬件：Ascend 910B
- CANN 版本：8.5.0
- 头文件：`/usr/local/Ascend/cann-8.5.0/include`
- 库路径：`/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64`

## 规则

- 只能查看 `agent_arena/` 目录下的文件
- 不要查看目录外的任何文件（ops-math/、error_testset/、ground_truth/ 等）
- 当前只审查 case_01，不要打开其他 case
