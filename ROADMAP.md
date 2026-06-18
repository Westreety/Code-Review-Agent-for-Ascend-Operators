# Pipeline 推进路线图

> 2026-06-16 | 基于当前状态（op_api 4/14，28.6%）的后续规划

---

## 一、目标分层

56 种错误不需要全部打通，按可行性分三层：

```
Tier 1: 可注入验证（~40 种）    ← 主战场
  一类~八类中标注 🟢易 / 🟡中 的可注入类型
  在 NPU 上实际注入 → 自验证 → Agent 评测

Tier 2: 归档观察（~12 种）      ← 记录不注入
  九类分布式通信（需多卡环境）
  十类编译器错误（需编译器环境）
  部分极难的六/七类

Tier 3: 不可注入（4 种）        ← 仅分类参考
  十一类硬件 SDC，超出代码修复范畴
```

---

## 二、分阶段路线

### Phase 1 — op_api 层（Mul 算子）

| 目标 | 当前 → 计划 | 投入 |
|------|:--:|------|
| 覆盖率 | 4/14 (28.6%) → 11/14 (78.6%) | 7 个新 case |

**第一批：易注入、高价值（4 个）**

| case | 编号 | 注入方式 | 难度 |
|------|:--:|------|:--:|
| A05 | 1.2 | 从 `ASCEND910B_DTYPE_DTYPE_SUPPORT_LIST` 删除 `DT_DOUBLE` | 🟢 |
| A06 | 1.3 | 添加 `DT_UINT32` 到 support list | 🟢 |
| A07 | 1.4 | `CheckMulNotNull` 校验失败时返回 `ACLNN_SUCCESS` | 🟢 |
| A08 | 4.2 | `CheckMulShape` 中删除 `OP_CHECK_MAX_DIM` 两行 | 🟢 |

**第二批：中等难度、高隐蔽性（3 个）**

| case | 编号 | 注入方式 | 难度 |
|------|:--:|------|:--:|
| A09 | 2.3 | `InferTensorScalarDtype` 中删除 `IsFloatEqual` 精度保持 | 🟡 |
| A10 | 2.4 | `CheckMulsPromoteDtype` 中删除 `inferDtype→out` 校验 | 🟡 |
| A11 | 2.2 | `InnerTypeToComplexType` 中 `BF16→COMPLEX64` 改为 `DT_UNDEFINED` | 🟡 |

**可选**

| case | 编号 | 注入方式 | 难度 |
|------|:--:|------|:--:|
| A12 | 1.5 | 只对 `CheckMulParams` 移除某项校验，`CheckInplaceMulParams` 保留 | 🟡 |

**Phase 1 完成状态**：op_api 11/14 = 78.6%，仅剩 3.1（设备路由，极难）、3.3（非连续内存，极难）未覆盖。

---

### Phase 2 — op_host 层（Mul 算子）

| 目标 | 当前 → 计划 | 投入 |
|------|:--:|------|
| 覆盖率 | 0/9 → 6/9 (67%) | 6 个新 case |

**涉及文件**：`mul_tiling_arch35.cpp` / `mul_infershape.cpp` / `mul_def.cpp` / `mul_proto.h`

| case | 编号 | 注入目标 | 方式 | 难度 |
|------|:--:|------|------|:--:|
| H01 | 5.1 | `mul_tiling_arch35.cpp` DTYPE_MAP | 删除一个 dtype 条目 | 🟢 |
| H02 | 5.6 | `mul_proto.h` Output dtype | 删除一种 dtype 声明 | 🟢 |
| H03 | 5.7 | `mul_binary.json` DynamicShape | 反转标志位 | 🟢 |
| H04 | 5.8 | `mul_def.cpp` opFile 字符串 | 指向不存在文件 | 🟢 |
| H05 | 5.3 | `mul_tiling_arch35.cpp` extraSize | `extraSize=0`（突破 32B 对齐） | 🟡 |
| H06 | 4.1 | `mul_infershape.cpp` BroadcastShape | 结果后篡改一维 shape | 🔴 |

---

### Phase 3 — op_kernel 层（Mul 算子）

| 目标 | 当前 → 计划 | 投入 |
|------|:--:|------|
| 覆盖率 | 0/21 → 8/21 (38%) | 8 个新 case |

**涉及文件**：`mul_dag.h` / `mul_apt.cpp`

**优先做（2 个）**

| case | 编号 | 注入目标 | 方式 | 难度 |
|------|:--:|------|------|:--:|
| K01 | 6.4 | `mul_apt.cpp` if constexpr 链 | 调换 `MulXfp16Op` 和 `MulOp` 分支 | 🟢 |
| K02 | 6.1 | `mul_dag.h` INT8 饱和截断 | 删除 `AndFF` 饱和操作 | 🟡 |

**后续扩展（6 个）**：6.2（NaN/Inf）、7.1（内存泄漏）、7.4（UB 超限）、7.5（TQue 死锁）、8.1（同步屏障）、8.3（Tiling 重叠）

---

### Mul 算子汇总

```
         已注入   Phase1  Phase2  Phase3  合计
op_api     4       +7       —       —     11/14  (78.6%)
op_host    0       —       +6       —      6/9   (66.7%)
op_kernel  0       —       —       +8      8/21  (38.1%)
─────────────────────────────────────────────────
合计       4       +7       +6      +8     25/56  (44.6%)
```

---

## 三、跨算子扩展策略（交替推进）

不等到 Mul 全部打完再扩展——每完成一层就在新算子上快速验证泛化能力。

```
Mul Phase 1 完成 → Add 算子 op_api 挑 3 个 top bug 快速注入
Mul Phase 2 完成 → Add 算子 op_host 挑 2 个验证
Mul Phase 3 完成 → Add/Div 全面铺开
```

**为什么交替而不是顺序**：
- 早期发现"只在 Mul 上成立的 bug 模式"
- 避免浪费 3 个 Phase 后才发现 Agent 换了算子就不行
- 注入模板化积累：`改 aclnn_mul.cpp 某行` → `改 {算子}_api 层 {校验函数}`

---

## 四、迭代优化嵌入点

| 阶段 | 优化什么 |
|------|------|
| Phase 1 完成 | eval_prompt 按层定制：tiling/kernel 层是否需要不同的审查指引？ |
| Phase 2 完成 | 评分体系建立：覆盖率 / 精度 / 误报率 三维量化 Agent 能力 |
| 扩展到 Add | 注入模板化：把手动改代码抽象为可复用的注入脚本 |
| 3+ 算子后 | 分类体系泛化验证：56 种中哪些是算子无关的通用类，哪些是特定算子特有 |

---

## 五、当前 Pipeline 流程

```
error_testset/              我们设计 + 自验证
  ├── 写 README.md + patch.diff + test_attack.cpp + run.sh
  ├── bash run.sh → NPU 上测试 → result_*.md
  └── 确认 bug 可触发

agent_arena/cases/          Agent 盲审评测
  ├── 复制代码到 op_api/AXX/aclnn_mul.cpp
  ├── bash deploy.sh AXX → 部署到 NPU
  ├── Agent 读 eval_prompt.md + cases/op_api/AXX/
  ├── Agent 写测试 + NPU 运行 + 出报告
  └── 输出到 output/op_api/AXX/result.md

人工分析
  ├── 去重、验证 Agent 发现
  ├── 更新 PROJECT_SUMMARY.md
  └── bash deploy.sh restore
```

---

## 六、新增 Case 操作清单

```
1. error_testset/op_api/1.X_xxx/       ← 创建目录 + README/patch/run.sh/test_attack
2. bash run.sh                          ← 自验证
3. agent_arena/cases/op_api/AXX/        ← 复制注入后代码
4. cases/INDEX.md                       ← 更新映射
5. bash deploy.sh AXX                   ← 部署到 NPU
6. Agent 评测                            ← 审查 + 测试 + 出报告
7. 分析结果，更新 PROJECT_SUMMARY.md
```
