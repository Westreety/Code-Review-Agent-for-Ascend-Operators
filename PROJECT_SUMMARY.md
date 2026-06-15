# Mul 算子错误注入与 Agent Code Review 评测项目

> **GitHub**: https://github.com/Westreety/Code-Review-Agent-for-Ascend-Operators
>
> 本文档为项目完整工作总结，v2.0 错误分类体系详见 `error_taxonomy/ERROR_TAXONOMY_v2.0.md`。

## 项目概述

本项目基于华为昇腾 CANN 平台的 Mul 算子（逐元素乘法），构建了一套完整的 **算子错误分类体系** 和 **Agent 代码审查评测框架**。通过系统性地注入不同类型错误，评估 AI Code Review Agent 的 bug 发现能力；同时使用无注入的原始代码，验证 Agent 能否发现源码中固有的缺陷。

**核心目标**：量化评估 AI Agent 在 NPU 算子代码审查中的能力边界。

---

## 一、项目结构

```
op_testing/
├── error_testset/                    # 错误分类文档
│   ├── ERROR_TAXONOMY.md             # 完整错误分类体系（8大类35种）
│   └── ERROR_CHEATSHEET.md           # 错误类型速查表
│
├── Mul算子/                          # 架构分析文档
│   ├── Mul 算子架构分析.md            # 三层架构详解
│   └── Mul 算子测试用例分析.md         # 测试用例设计指南
│
├── agent_arena/                      # Agent 评测主目录
│   ├── eval_prompt.md                # 评测提示词（通用）
│   ├── eval_prompt_custom.md         # 自定义算子评测提示词
│   ├── README.md                     # 评测说明
│   ├── examples/                     # 测试模板
│   │   ├── test_aclnn_mul.cpp        # 官方 Mul 测试模板
│   │   ├── test_custom_mul.cpp       # 自定义算子测试模板
│   │   └── build.sh                  # 编译脚本
│   ├── cases/                        # 评测用例
│   │   ├── case_01/aclnn_mul.cpp     # 注入：空指针校验缺失
│   │   ├── case_02/aclnn_mul.cpp     # 注入：out shape 校验缺失
│   │   ├── case_03/aclnn_mul.cpp     # 注入：DT_DOUBLE 硬编码拒绝
│   │   ├── case_04/aclnn_mul.cpp     # 注入：空 tensor 检查缺失
│   │   ├── case_00/aclnn_mul.cpp     # 无注入（官方原始代码）
│   │   └── case_00_2.0/aclnn_mul.cpp # 无注入（独立重复实验）
│   ├── custom_op/                    # 自定义算子（Heisenbug 探索）
│   │   └── custom_mul/               # 手写流水线的 CustomMul
│   │       ├── op_api/               # L0 接口层
│   │       ├── op_host/              # Tiling + 注册
│   │       ├── op_kernel/            # Kernel 实现
│   │       ├── op_graph/             # 图 IR
│   │       └── CMakeLists.txt
│   └── output/                       # Agent 输出结果
│       ├── case_01/ ~ case_04/       # 注入评测结果
│       ├── case_00/                  # 无注入评测结果
│       └── case_00_2.0/              # 无注入重复实验结果
│
└── custom_op/                        # 自定义算子源码（开发用）
    └── custom_mul/
```

**参考代码库**：`ops-math/math/mul/` — CANN 官方 Mul 算子实现，作为对照基准。

---

## 二、错误分类体系

基于 `ops-math/math/mul/` 源码的完整分析，将 Mul 算子可能出现的错误划分为 **11 大类 56 种**，覆盖 op_api → op_host → op_kernel → 编译器 → 硬件全栈五层架构。

> **版本**：v3.0（2026-06-15）
> - v1.0：8 大类 35 种，覆盖 op_api/op_host/op_kernel
> - v2.0：新增九类（分布式通信，4种）、六类新增 6.6（Cube/Vector 精度截断）、七类新增 7.4（L1/UB 超限）、八类新增 8.6（死锁）+ 8.7（Padding 污染），合并 8.1（含 DMA 竞争）+ 8.3（含跨核写污染）
> - v3.0：新增十类（编译器静默错误，3种）+ 十一类（硬件 SDC，4种），七类扩充 TQue 死锁 + 未释放 Tensor 挂起，八类扩充跨核 Barrier 死锁，九类扩充流毒化机制 + 多流资源饥饿，五类扩充 UB 对齐约束 + Stride Copy，总计 11 大类 56 种
> - 存档：`error_taxonomy/ERROR_TAXONOMY_v1.0.md`、`ERROR_TAXONOMY_v2.0.md`、`ERROR_TAXONOMY_v3.0.md`

### 2.1 分类总览

```
一类：参数校验类错误（5种）
  ├── 1.1 空指针校验缺失
  ├── 1.2 dtype 白名单遗漏（合法 dtype 误杀）
  ├── 1.3 dtype 白名单过宽（非法 dtype 放行）
  ├── 1.4 错误码伪装
  └── 1.5 API 变体间校验逻辑不一致

二类：类型推导/提升类错误（4种）
  ├── 2.1 混合精度组合被误删
  ├── 2.2 Complex 类型推导回退错误
  ├── 2.3 Tensor×Scalar 精度保持判断丢失
  └── 2.4 类型转换可逆性检查缺失

三类：路由与调度错误（3种）
  ├── 3.1 设备路由错误（AiCore/AiCpu 选择失误）
  ├── 3.2 空 Tensor 处理遗漏
  └── 3.3 非连续内存优化路径错误

四类：Shape/广播类错误（3种）
  ├── 4.1 广播 Shape 计算错误
  ├── 4.2 维度上限检查缺失
  └── 4.3 输出 Shape 校验缺失

五类：Tiling 策略与硬件边界冲突（8种）
  ├── 5.1 Tiling 策略表条目缺失
  ├── 5.2 Tiling 策略映射错误
  ├── 5.3 UB 物理地址 32B 对齐约束违例
  ├── 5.4 Stride Copy 引发隐式内存膨胀
  ├── 5.5 UB 大小获取错误
  ├── 5.6 dtype 注册跨层不一致
  ├── 5.7 AiCore 编译配置错误
  └── 5.8 OpFile 名称错误

六类：计算执行类错误（7种）
  ├── 6.1 整数溢出未处理
  ├── 6.2 特殊浮点值传播错误（NaN/Inf/Mask）
  ├── 6.3 Double 精度实现错误
  ├── 6.4 if constexpr 分支顺序错误
  ├── 6.5 跨层 dtype 声明不一致
  ├── 6.6 Cube/Vector 中间精度截断
  └── 6.7 算术中间累加器溢出

七类：资源管理类错误（6种）
  ├── 7.1 算子侧内存泄漏
  ├── 7.2 Stream 同步缺失
  ├── 7.3 测试侧资源泄漏
  ├── 7.4 L1/UB 分配超限与碎片化
  ├── 7.5 TQue 深度失配与挂起死锁
  └── 7.6 未释放 Tensor 造成的单核挂起

八类：非确定性计算错误 / Heisenbug（8种）
  ├── 8.1 异步 DMA 与计算流的 RAW/WAW 冒险
  ├── 8.2 脏内存/未初始化内存
  ├── 8.3 Tiling 分块重叠（含跨核边界写污染）
  ├── 8.4 原子操作缺失
  ├── 8.5 CPU-NPU 缓存一致性延迟与乱序
  ├── 8.6 多级流水线 EnQue/DeQue 顺序错乱与死锁
  ├── 8.7 内存对齐与 Padding 残留污染
  └── 8.8 跨核全局 Barrier 死锁

九类：分布式集群网络通信与拓扑调度（5种）
  ├── 9.1 设备断言导致流毒化与级联超时
  ├── 9.2 多流资源竞争导致通信算子饥饿
  ├── 9.3 跨节点 Shape/Rank 不一致死锁
  ├── 9.4 集合通信精度截断错误
  └── 9.5 算子流与通信流同步缺失

十类：软硬协同与中间件图编译错误（3种）
  ├── 10.1 算子融合图优化导致非法内存别名
  ├── 10.2 Eager 与 Graph 模式运行态语义分歧
  └── 10.3 Triton/IR 编译器后端符号计算崩溃

十一类：底层硬件静默数据损坏 / SDC（4种）
  ├── 11.1 组合逻辑位翻转与无 NaN 损坏
  ├── 11.2 Warp 对齐的空间周期性计算错误
  ├── 11.3 电压/热节流导致的微秒级时序错乱
  └── 11.4 算力节点亚健康诱发全网梯度发散
```

### 2.2 注入难度与可检测性统计

| 类别 | 子类型数 | 简单注入 | 中等注入 | 困难/极难注入 | 不可注入 |
|------|:------:|:------:|:------:|:------:|:------:|
| 一类·参数校验 | 5 | 4 | 1 | 0 | 0 |
| 二类·类型推导 | 4 | 0 | 4 | 0 | 0 |
| 三类·路由调度 | 3 | 1 | 0 | 2 | 0 |
| 四类·Shape/广播 | 3 | 2 | 0 | 1 | 0 |
| 五类·Tiling+注册 | 8 | 4 | 2 | 2 | 0 |
| 六类·计算执行 | 7 | 1 | 0 | 6 | 0 |
| 七类·资源管理 | 6 | 1 | 1 | 4 | 0 |
| 八类·非确定性 | 8 | 0 | 2 | 6 | 0 |
| 九类·分布式通信 | 5 | 0 | 0 | 5 | 0 |
| 十类·编译器 | 3 | 0 | 0 | 3 | 0 |
| 十一类·硬件SDC | 4 | 0 | 0 | 0 | 4 |
| **合计** | **56** | **13** | **10** | **29** | **4** |

| 可检测性 | 数量 | 代表编号 |
|:------:|:---:|---------|
| 🟢 易 | 12 | 1.1, 1.2, 1.4, 2.2, 3.2, 4.3, 5.1, 5.6, 5.7, 5.8, 7.3 |
| 🟡 中 | 8 | 1.3, 2.1, 2.4, 3.3(结果), 4.2, 6.1, 6.5, 7.4 |
| 🔴 难/极难 | 32 | 1.5, 2.3, 3.1, 3.3(perf), 4.1, 5.2~5.5, 6.2~6.4, 6.6, 6.7, 7.1, 7.2, 7.5, 7.6, 8.1~8.8, 9.1~9.5, 10.1~10.3 |
| ⬛ 不可注入 | 4 | 11.1~11.4 |

---
| 🔴 难 | 20 | 2.3, 3.1, 3.3(perf), 4.1, 5.2, 5.3, 6.2, 6.3, 6.4, 6.5, 6.6, 7.1, 7.2, 8.1~8.9, 9.1~9.4 |

---

## 三、评测框架设计

### 3.1 评测流程

```
1. 准备 case 代码（注入 bug 或使用原始代码）
2. 部署算子到 NPU
3. Agent 阅读 eval_prompt.md 了解任务
4. Agent 审查 cases/ 下的代码
5. Agent 编写独立测试程序
6. Agent 在 NPU 上编译运行测试
7. Agent 输出报告到 output/
8. 人工分析 Agent 报告，去重、验证
```

### 3.2 关键设计原则

- **子测试独立**：每个 bug 一个独立二进制，崩溃不影响后续测试
- **NPU 验证强制**：每个 bug 必须在 NPU 上编译运行，不允许仅代码分析
- **无记忆规则**：Agent 不参考外部记忆或历史会话，仅基于当前目录代码
- **通用审查方法**：审查方法不针对特定 bug 类型，保持通用性

### 3.3 环境信息

| 项目 | 值 |
|------|-----|
| 硬件 | Ascend 910B |
| CANN | 8.5.0 |
| 算子源码 | `ops-math/math/mul/op_api/aclnn_mul.cpp` |
| 评测文件 | `agent_arena/cases/case_XX/aclnn_mul.cpp` |
| NPU 算子库 | `vendors/custom_math/op_api/lib/libcust_opapi.so` |

---

## 四、op_api 层评测：注入与无注入 Code Review

> 评测对象：`aclnn_mul.cpp`（Mul 算子的 L2 API 层，694 行），部署在 Ascend 910B NPU 上。

### 4.1 评测设计

共进行了 **6 轮** Agent 评测：

| 类型 | case | 说明 |
|:--:|:--:|------|
| 注入 | case_01~04 | 在 `aclnn_mul.cpp` 中注入 4 种不同类型的 bug |
| 无注入 | case_00, case_00_2.0 | 使用 ops-math 官方原始代码，不做任何修改 |

**注入设计**：

| case | 注入改动 | 错误类型 | diff |
|:--:|------|------|------|
| 01 | `OP_CHECK_NULL(out)` → `(void)out` | 1.1 空指针校验缺失 | `:143` |
| 02 | `OP_CHECK_SHAPE_NOT_EQUAL(out, dstShape)` → `(void)out` | 4.3 输出 Shape 校验缺失 | `:297` |
| 03 | 插入 `if (DT_DOUBLE) return ACLNN_ERR_PARAM_INVALID` | 2.1 混合精度组合被误删 | `:465` |
| 04 | 删除空 tensor 检查代码块 | 3.2 空 Tensor 处理遗漏 | `:466-472` |

### 4.2 评测结果

**注入 case 结果**：

| case | 注入 bug 发现 | 额外 bug 发现 | 额外 bug 编号 |
|:--:|:--:|:--:|------|
| 01 | ✅ | 0 | — |
| 02 | ✅ | 3 | S1, S2, S4 |
| 03 | ✅ | 2 | S2, S6 |
| 04 | ✅ | 3 | S2, S3, S5 |

所有 case 均成功发现了注入的 bug，case_02 的额外发现最丰富。

**无注入 case 结果**：

| case | 额外 bug 发现 | 额外 bug 编号 |
|:--:|:--:|------|
| 00 | 4 | S3, S4, S5, S7 |
| 00_2.0 | 4 | S1, S2, S8, S9 |

两次无注入评测结果差异巨大（仅 S3 重合），验证了 LLM 审查的非确定性。

### 4.3 Agent 发现的源码固有 Bug（9 个）

以下 bug 均在 ops-math 官方原始 `aclnn_mul.cpp` 中存在，非人工注入。经 NPU 编译运行验证。

| # | Bug | 代码位置 | 运行时影响 | 兜底机制 | 发现于 |
|:--:|------|------|:--:|------|:--:|
| **S1** | CombineCategoriesWithComplex 整型溢出 | `:104-106` | **有** | 无 | case_02, case_00_2.0 |
| **S2** | RegBase 跳过 out dtype 校验 | `:269-272` | **有** | 无 | case_02/03/04, case_00_2.0 |
| **S3** | InplaceMulShape 缺 MAX_DIM | `:301-306` | **有** | 无 | case_04, case_00 |
| **S8** | ToFloat() 非浮点 scalar 精度丢失 | `:207` | **有** | 无 | case_00_2.0 |
| S4 | InnerTypeToComplexType F16→C32 | `:70` | 无 | `:210-212` 显式兜底 | case_02, case_00 |
| S5 | promoteType 缺 support list 校验 | `:260-290` | 低 | 下游 Cast 路径 | case_04, case_00 |
| S6 | MulsDtype 未校验 scalar dtype | `:159` | 无 | InferTensorScalarDtype 推导链 | case_03 |
| S7 | InplaceMuls 缺 MulsCheckFormat | `:541` | 低 | 仅日志缺失 | case_00 |
| S9 | IsMulMixDtypeSupport 缺 BF16×F16 | `:355-360` | 低 | Cast 路径兜底 | case_00_2.0 |

**各 case 发现矩阵**：

| # | case_01 | case_02 | case_03 | case_04 | case_00 | case_00_2.0 |
|:--:|:--:|:--:|:--:|:--:|:--:|:--:|
| S1 | | ✅ | | | | ✅ |
| S2 | | ✅ | ✅ | ✅ | | ✅ |
| S3 | | | | ✅ | ✅ | |
| S4 | | ✅ | | | ✅ | |
| S5 | | | | ✅ | ✅ | |
| S6 | | | ✅ | | | |
| S7 | | | | | ✅ | |
| S8 | | | | | | ✅ |
| S9 | | | | | | ✅ |
| **合计** | **0** | **3** | **2** | **3** | **4** | **4** |

**NPU 验证**：

| Bug | 测试输入 | NPU 实际 | 结论 |
|------|------|------|:--:|
| S1 | INT8×INT32 scalar | 100×2=-56（溢出） | 确认 |
| S2 | BOOL×BOOL→DOUBLE | status=0（通过） | 确认 |
| S3 | 9 维 InplaceMul | status=0（通过） | 确认 |
| S8 | BF16×INT64_MAX | 9.22e18（非精确值） | 确认 |
| S4 | FLOAT16×COMPLEX64 | COMPLEX32→64 修正生效 | 兜底确认 |
| S6 | FLOAT×UINT32 scalar | 类型推导链隐式处理 | 兜底确认 |

### 4.4 关键发现

1. **注入 bug 能引导 Agent 关注同类问题**：case_02 发现的额外 bug 最多
2. **无注入审查覆盖不稳定**：case_00 和 case_00_2.0 仅 1 个 bug 重合
3. **4 个有运行时影响的 bug 无兜底**：S1/S2/S3/S8 影响计算结果
4. **5 个被兜底或影响极低**：S4/S6 有显式兜底，S5/S7/S9 仅影响性能或可观测性

### 4.5 op_api 层错误类型覆盖情况

根据错误分类体系，op_api 层（`aclnn_mul.cpp`）共有 **14 种** 可注入的错误类型。当前仅覆盖了 **4 种**。

| 类别 | 编号 | 错误类型 | 是否已注入 | 对应 case |
|------|:--:|------|:--:|:--:|
| 一类·参数校验 | 1.1 | 空指针校验缺失 | ✅ | case_01 |
| | 1.2 | dtype 白名单遗漏 | ❌ | — |
| | 1.3 | dtype 白名单过宽 | ❌ | — |
| | 1.4 | 错误码伪装 | ❌ | — |
| | 1.5 | API 变体间校验不一致 | ❌ | — |
| | 1.6 | 格式检查缺失/错误 | ❌ | — |
| 二类·类型推导 | 2.1 | 混合精度组合被误删 | ✅ | case_03 |
| | 2.2 | Complex 类型推导回退错误 | ❌ | — |
| | 2.3 | Tensor×Scalar 精度保持判断丢失 | ❌ | — |
| | 2.4 | 类型转换可逆性检查缺失 | ❌ | — |
| 三类·路由调度 | 3.2 | 空 Tensor 处理遗漏 | ✅ | case_04 |
| 四类·Shape/广播 | 4.2 | 维度上限检查缺失 | ❌ | — |
| | 4.3 | 输出 Shape 校验缺失 | ✅ | case_02 |

> 注：三类中的 3.1（设备路由错误）在 `mul.cpp` 中，3.3（非连续内存优化）在 `aclnn_mul.cpp` 中但注入难度高。四类中的 4.1（广播 Shape 计算错误）在 `mul_infershape.cpp` 中。

**覆盖情况**：已注入 4 种 / 可注入 14 种 = **28.6%**

**未覆盖的高价值类型**：
- 1.4 错误码伪装：校验失败返回 SUCCESS，后续崩溃
- 1.5 API 变体间校验不一致：不同 API 行为不对称（Agent 已发现此为源码固有 bug S3/S7）
- 2.3 Scalar 精度保持丢失：精度敏感，隐蔽性高
- 2.4 类型转换可逆检查缺失：非法类型转换静默通过

---

## 八、自定义算子 Heisenbug 探索

### 8.1 背景

第八类（非确定性计算错误/Heisenbug）是 NPU 异构计算中最难捕获的缺陷类型。为探索 Agent 对此类错误的检测能力，设计了一个自定义算子 `CustomMul`。

### 8.2 CustomMul 架构

```
custom_mul/
├── op_api/custom_mul.{h,cpp}       # L0 接口层（AiCore 路由）
├── op_host/
│   ├── arch35/custom_mul_tiling.{h,cpp}  # 简化的 Tiling（等长切分，1024元素/block）
│   ├── custom_mul_def.cpp          # 算子注册（仅 FLOAT32）
│   ├── custom_mul_infershape.cpp   # Shape 推导（广播）
│   └── config/ascend950/           # 芯片配置
├── op_kernel/
│   ├── custom_mul_apt.cpp          # Kernel 入口（__aicore__）
│   └── arch35/
│       ├── custom_mul_dag.h        # 手写 TPipe 流水线（不含 BroadcastSch 框架）
│       └── custom_mul_struct.h     # Tiling key 定义
├── op_graph/custom_mul_proto.h     # GE IR 注册
└── CMakeLists.txt                  # 构建配置
```

与官方 Mul 算子的关键区别：Kernel 层使用手写 `TPipe` 流水线而非 `BroadcastSch` 框架，开发者需要自己管理同步。

### 8.3 Heisenbug 实现结论

| 子类型 | 能否在 Ascend C 框架中真实触发 | 结论 |
|:--:|:--:|------|
| 8.1 同步屏障缺失 | ❌ | `DataCopy` 后缺 `SyncAll` 导致确定性崩溃，非偶发 |
| 8.2 脏内存/未初始化 | ⚠️ 未完成 | Kernel 编译问题未解决 |
| 8.3 Tiling 分块重叠 | ✅ 待设计 | 唯一可真实触发的 Heisenbug |
| 8.4 原子操作缺失 | ❌ | Mul 算子无多 core 共享写入模式 |
| 8.5 内存序问题 | ⚠️ | 可触发但不稳定 |

---

## 九、评测基础设施

### 9.1 评测提示词

`eval_prompt.md`（通用）核心要求：

- 审查代码，列出所有 bug（位置、类型、严重程度）
- 每个 bug 编写独立测试程序
- **每个 bug 必须在 NPU 上编译运行验证**
- 给出修复方案（diff 格式）
- 不允许仅代码分析而不提供 NPU 证据
- 独立审查，不参考外部记忆

### 9.2 测试模板

- `examples/test_aclnn_mul.cpp`：标准 aclnn 两段式 API 调用模板
- `examples/test_custom_mul.cpp`：L0 层 API 调用模板（自定义算子用）
- `examples/build.sh`：编译脚本

### 9.3 部署

- 官方 Mul 算子：通过 `vendors/custom_math/op_api/lib/libcust_opapi.so`
- 自定义 CustomMul：通过 `vendors/custom_test_math/op_api/lib/libcust_opapi.so`

---

## 十、项目产出

| 产出 | 文件 | 状态 |
|------|------|:--:|
| 错误分类体系 | `ERROR_TAXONOMY.md` | ✅ 完成 |
| 错误速查表 | `ERROR_CHEATSHEET.md` | ✅ 完成 |
| 架构分析文档 | `Mul 算子架构分析.md` | ✅ 完成 |
| 测试用例分析文档 | `Mul 算子测试用例分析.md` | ✅ 完成 |
| 评测提示词 | `eval_prompt.md` | ✅ 完成 |
| 自定义算子提示词 | `eval_prompt_custom.md` | ✅ 完成 |
| 注入评测 4 个 case | `cases/case_01~04` | ✅ 完成 |
| 无注入评测 2 个 case | `cases/case_00, case_00_2.0` | ✅ 完成 |
| 9 个源码固有 bug 验证 | NPU 测试 | ✅ 完成 |
| 自定义算子 CustomMul | `custom_op/custom_mul/` | ✅ 完成 |
| Heisenbug 分类探索 | 8.1~8.5 | ⚠️ 部分完成 |

---

## 十一、后续工作方向

1. **8.3 Tiling 分块重叠**：设计并验证唯一可真实触发的 Heisenbug
2. **多轮评测**：更多 Agent 实例评测以评估覆盖率稳定性
3. **其他算子扩展**：将评测框架应用于 Add、Div 等其他 CANN 算子
4. **Agent 评分体系**：建立量化的 Agent 能力评分标准（覆盖率、精度、误报率）
5. **自动兜底分析**：工具化检查 bug 是否被下游代码兜底
