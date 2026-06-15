# 项目总结：Mul 算子 Code Review Agent 评测体系

> 本文件为多轮对话的工作浓缩，供新会话快速上手。包含所有关键决策、已验证结论和当前状态。

---

## 1. 项目定位

### L1 → L2 → L3 三层架构

```
L1: 算子代码（Mul 算子，正确的或注入 bug 的）
L2: Code Review Agent（未来要评测它能否识别 bug）
L3: 对 Agent 的评测（我们当前在做的事：构建"注入了已知 bug 的算子"，验证 Agent 能力）
```

### 当前阶段

**POC 阶段**：以 Mul 算子为例，构建错误算子 + 配套测试用例 + 自动化 pipeline，验证 Code Review Agent 的 bug 检测能力。

---

## 2. 环境信息

| 项目 | 值 |
|------|-----|
| NPU | Ascend 910B，8卡 |
| CANN 版本 | 8.5.0 |
| 容器路径 | `/models/chenjunyang/workspace/ops-math`（持久化，不用 /home） |
| SOC | ascend910 |
| 头文件 | `/usr/local/Ascend/cann-8.5.0/include` |
| 库文件 | `/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64` |
| 链接库 | `-lascendcl -lnnopbase -lcust_opapi` |
| 算子库路径 | `/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_math/op_api/lib` |

---

## 3. Mul 算子架构（三层）

```
op_api 层（CPU）— aclnn_mul.cpp (694行)
  参数校验（空指针、dtype白名单、PromoteType、Shape）
  空tensor处理、设备路由、非连续内存优化

op_host 层（CPU）— mul_infershape.cpp(47) + mul_def.cpp(72) + mul_tiling_arch35.cpp(251)
  Shape推断/广播、算子注册、Tiling策略调度
  ⚠️ mul_tiling_arch35.cpp 的编译产物不部署到 NPU（编译进预编译系统 .so）

op_kernel 层（NPU）— mul_dag.h(507) + mul_apt.cpp(70) + mul_struct.h(27)
  8 条计算流水线：Int8/Uint8/Bool/Xfp16/Complex32/Mul通用/Double/MixFp
  TBE 源码部署，改后 build.sh 打包可生效
```

---

## 4. 错误分类体系（8 大类 × 35 种子类型）

> 详细文件：`Mul算子/error_testset/ERROR_TAXONOMY.md`

| 类别 | 子类型数 | 主注入层 | 特征 |
|------|:--:|------|------|
| 一类：参数校验 | 6 | op_api | 空指针、dtype白名单、错误码、API变体、格式 |
| 二类：类型推导 | 4 | op_api | 混精组合、Complex、Scalar精度、类型转换 |
| 三类：路由调度 | 3 | op_api | 设备路由、空tensor处理、非连续内存 |
| 四类：Shape检查 | 3 | op_api/host | 广播shape、维度上限、输出校验 |
| 五类：Tiling+注册 | 6 | op_host | 策略缺失/错指、UB大小、dtype注册、AiCore配置、OpFile |
| 六类：计算执行 | 5 | op_kernel | 整数溢出、浮点NaN/Inf、Double精度、分支顺序、跨层不一致 |
| 七类：资源管理 | 3 | 跨层 | 内存泄漏、Stream同步、测试侧资源 |
| 八类：**非确定性** | 5 | kernel/跨层 | 同步屏障、脏内存、Tiling重叠、原子操作、内存序 |

### 难度分布

| 可检测性 | 数量 | 代表 |
|:--|:--:|------|
| 🟢 易 | 12 | 1.1 空指针、4.3 shape、5.4~5.6 注册配置 |
| 🟡 中 | 8 | 1.3 白名单过宽、2.1 混精组合、4.2 维度上限 |
| 🔴 难 | 13 | 六类全部计算执行 + 七类资源管理 + 八类全部海森堡 |

---

## 5. POC 第一批（4 个 case，全部 NPU 实测通过 ✅）

> 文件在：`Mul算子/error_testset/case_XX_*/`

| Case | 错误类型 | 注入位置 | NPU 实测 |
|:--:|------|------|:--:|
| case_01 | 1.1 空指针校验缺失 | `CheckMulNotNull()` 删 `OP_CHECK_NULL(out)` | SEGFAULT (exit=139) |
| case_02 | 4.3 输出Shape校验缺失 | `CheckMulShape()` 删输出shape检查 | 错误 561103 |
| case_03 | 5.1 DOUBLE类型拦截 | `aclnnMulGetWorkspaceSize()` 插入 DT_DOUBLE 拦截 | 成功拒绝 |
| case_04 | 3.2 空Tensor处理遗漏 | `aclnnMulGetWorkspaceSize()` 删 IsEmpty() | aclrtMalloc 失败 |

### 每个 case 包含文件

```
case_XX/
├── run.sh              ← 自动化脚本：\cp备份 → sed注入 → build.sh编译 → 安装 → 测试 → 自动还原
├── test_attack.cpp     ← 攻击测试代码（examples 风格，含 PASS/FAIL 输出）
├── buggy_aclnn_mul.cpp ← 静态度量：注入后的 buggy 文件（用于 diff 对比）
├── README.md           ← 错误说明
└── result_*.md         ← NPU 运行生成的测试报告（Markdown）
```

### 自动化 Pipeline（run.sh 流程）

```
[1/5] \cp .bak 备份 → sed 单行注入 → touch 强制重编
[2/5] build.sh --pkg --ops=mul → 保留 build/ 增量编译
[3/5] install .run 包到 CANN
[4/5] g++ 编译 test_attack.cpp → NPU 执行 → 捕获退出码
trap  → \cp .bak 还原源码
```

---

## 6. Agent 评测 Pipeline

> 文件在 `Mul算子/agent_eval/`

```
agent_eval/
├── README.md                           ← 评测说明 + 评分标准（0/1/2 三维度，满分6分，通过线4分）
├── eval_prompt.md                      ← 统一发给 Agent 的评测任务 prompt
└── cases/
    ├── case_01_nullptr_check/
    │   ├── buggy_aclnn_mul.cpp         ← 给 Agent 的 buggy 代码（不含答案）
    │   └── ground_truth.md             ← 评分对照答案（我们自己用）
    ├── case_02_output_shape/
    ├── case_03_tiling_missing/
    └── case_04_empty_tensor/
```

### 评分维度

| 维度 | 0分 | 1分 | 2分 |
|------|-----|-----|-----|
| bug 发现 | 未发现 | 位置大致对，类型不准 | 文件+函数+行号，类型正确 |
| 测试生成 | 无 | 有框架但无法触发 | 精确触发 bug |
| 解释修复 | 错 | 思路对，细节不全 | 根因+正确修复 |

---

## 7. 关键技术教训

| 教训 | 详情 |
|------|------|
| **Tiling 代码改不动** | `mul_tiling_arch35.cpp` 编译进预编译系统 .so，不部署到 NPU。case_03 改到 aclnn_mul.cpp 才生效 |
| **cp -i 别名陷阱** | 容器 `cp` 被别名成 `cp -i`，脚本需 `\cp` 绕过 |
| **管道吞退出码** | `\| tee` 会吞掉测试程序退出码，需用 `${PIPESTATUS[0]}` |
| **sed 锚点必须唯一** | 不能用通用注释做锚点（如 "// 混合输入类型" 在两处出现），需用函数特有的行 |
| **build/ 增量编译** | 保留 `build/` 目录，`touch` 触发重编 → 后续 case <30s |
| **architecture 问题** | CANN 8.5.0 是 aarch64，安装包名是 `cann-ops-math-custom_linux-aarch64.run` |
| **op_kernel 层可改** | mul_dag.h + mul_apt.cpp 走 TBE 源码部署，改完 build.sh 直接生效，和 aclnn_mul.cpp 一样 |

---

## 8. 当前进度与下一步

### 已完成

- ✅ Mul 算子三层架构分析
- ✅ 8 类 35 种错误类型完整分类（ERROR_TAXONOMY.md）
- ✅ 4 个 POC case 在 NPU 上全部跑通（BUG DETECTED）
- ✅ Agent 评测 Pipeline 搭建完成（agent_eval/）
- ✅ 项目汇报 PPT 生成（`项目汇报.pptx`）

### 下一步建议

1. **🔴 立即**：用 agent_eval/ 的 4 个 case 跑第一轮 Agent 评测，验证输入格式和评分标准
2. **🟡 近 1 周**：补完 op_api 剩余 9 个子类型（1.2~1.6, 2.1~2.4, 3.1, 3.3）→ 每个只需改 sed 行
3. **🟡 探索**：验证 kernel 层注入（改 mul_dag.h / mul_apt.cpp → build.sh → NPU 是否生效）
4. **🔵 远期**：海森堡错误（第八类）的评测设计与代理注入实验

---

## 9. 关键文件索引

| 文件 | 路径 | 说明 |
|------|------|------|
| 错误分类总表 | `error_testset/ERROR_TAXONOMY.md` | 8类35种，含架构图、注入点、行号 |
| 速查表 | `error_testset/ERROR_CHEATSHEET.md` | 三维交叉矩阵 |
| Agent评测入口 | `agent_eval/README.md` | 评测说明 + 评分标准 |
| Agent评测prompt | `agent_eval/eval_prompt.md` | 统一发给Agent的prompt |
| 项目汇报PPT | `error_testset/项目汇报.pptx` | 15页，深蓝主题 |

---

> 生成时间：2026-06-05
> 环境：Windows 本地 + 远端 Ascend 910B NPU (SSH)
> 准备在新机器上使用：只需 scp agent_eval/ 目录到 NPU 容器即可开始评测
