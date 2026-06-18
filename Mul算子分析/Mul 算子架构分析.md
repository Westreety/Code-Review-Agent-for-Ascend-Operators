## 1 引言

本文以 ops-math 仓库中的 Mul（逐元素乘法）算子为例，分析 CANN 平台算子的工程结构与分层设计。

Mul 算子的数学定义为 y=x1×x2y = x_1 \times x_2y=x1​×x2​，其中 x1x_1x1​、x2x_2x2​ 为输入张量，yyy 为输出张量。当两个输入的 shape 不一致时，按广播（broadcast）规则对齐后再逐元素计算。数学定义虽然简单，但在 NPU 上实现该算子需要处理 11 种数据类型、多种 shape 组合、硬件内存约束和特殊浮点值等工程问题，因此实现被拆分为多个层次。

CANN 中的其他算子（如 Add、Abs、Div 等）均遵循与 Mul 相同的目录结构和分层设计。掌握 Mul 的架构后，读者可以将相同的分析方法应用于仓库中的任意算子。

## 2 目录结构

Mul 算子位于 `math/mul/` 目录下：

```
math/mul/
├── op_api/                         # 接口层
│   ├── aclnn_mul.h                 #   四个 API 的函数声明
│   ├── aclnn_mul.cpp               #   参数校验、类型提升、调度（694行）
│   ├── mul.h / mul.cpp             #   底层接口与设备路由（146行）
├── op_host/                        # 主机计算层
│   ├── mul_def.cpp                 #   算子注册：声明支持的 dtype 组合
│   ├── mul_infershape.cpp          #   shape 推断（47行）
│   └── arch35/
│       └── mul_tiling_arch35.cpp   #   tiling 切分策略（251行）
├── op_kernel/                      # 设备计算层
│   ├── mul_apt.cpp                 #   kernel 入口：编译期类型分发（69行）
│   └── arch35/
│       └── mul_dag.h               #   核心计算逻辑：8种计算流水线（502行）
├── op_graph/                       # 图优化层（本次实验不涉及）
├── examples/                       # 使用示例
└── tests/                          # 单元测试
```

这些文件可以划分为三个主要层次：op_api（接口层）、op_host（主机计算层）和 op_kernel（设备计算层）。下面从一次 `aclnnMul` 调用的执行顺序出发，逐层分析各层的职责。

## 3 分层架构

当用户调用 `aclnnMul(x1, x2, out)` 时，执行流程依次经过三层：

```
用户调用 aclnnMul(x1, x2, out)
         │
         ▼
┌──────────────────────────────────┐
│  op_api 层   运行于 CPU           │
│  参数校验 → 类型提升 → 路径选择    │
└────────────────┬─────────────────┘
                 │
                 ▼
┌──────────────────────────────────┐
│  op_host 层  运行于 CPU           │
│  shape 推断 → tiling 切分计算     │
└────────────────┬─────────────────┘
                 │
                 ▼
┌──────────────────────────────────┐
│  op_kernel 层  运行于 NPU         │
│  按切分方案执行逐元素乘法          │
└──────────────────────────────────┘
```

### 3.1 op_api 层

op_api 是用户直接调用的入口。它的主要工作分为三个阶段。

**参数校验阶段**，该层检查输入是否合法：空指针、不支持的数据类型（如 UINT32）、无法广播的 shape 组合、输出类型与推导结果不一致等情况都会在此被拦截，返回相应的错误码。

**类型提升阶段**，当两个输入的数据类型不同时（例如 `float16 * float32`），op_api 需要决定以何种精度进行中间计算。该层根据类型提升规则确定计算类型，并在必要时插入类型转换操作。

**路由阶段**，`mul.cpp` 中的逻辑根据数据类型和硬件能力，决定将计算派发至 AI Core（专用计算单元）还是 AI CPU（通用计算单元）。

此外，Mul 算子对外提供四个 API，覆盖不同的使用场景：

|API|语义|
|---|---|
|`aclnnMul(self, other, out)`|out = self * other（tensor 乘 tensor）|
|`aclnnMuls(self, other, out)`|out = self * scalar（tensor 乘标量）|
|`aclnnInplaceMul(selfRef, other)`|selfRef *= other（原地乘 tensor）|
|`aclnnInplaceMuls(selfRef, other)`|selfRef *= scalar（原地乘标量）|

这四个 API 在 op_api 层的调度逻辑各不相同，因此在测试中需要分别覆盖。

`aclnn_mul.cpp` 是 op_api 层中代码量最大、条件分支最多的文件（694 行，约 20 个条件分支）。其中的分支主要来源于：是否为混合 dtype（如 float16 与 float32 的组合）、是否需要类型转换、输入内存是否连续、以及不同硬件平台的能力差异等。通过使用不同的 dtype 组合、不同的 API 变体、以及异常输入，可以覆盖该文件中的不同分支。

### 3.2 op_host 层

op_api 完成参数校验后，执行流进入 op_host 层。该层在 CPU 侧完成两项准备工作。

**Shape 推断**（`mul_infershape.cpp`）根据广播规则计算输出的 shape。广播规则从最后一个维度开始逐维比对，要求每对维度要么相等、要么其中一个为 1。例如 `[2,3]` 与 `[3]` 广播后为 `[2,3]`，而 `[2,3]` 与 `[4,5]` 无法广播。

**Tiling 切分**（`mul_tiling_arch35.cpp`）解决的是硬件内存约束问题。NPU 的 AI Core 片上缓存（Unified Buffer）容量通常只有数百 KB，而输入 tensor 的数据量可能远超这一容量。Tiling 层根据 UB 大小、数据类型和 shape，计算出将 tensor 分成多少块、每块搬运多少数据、循环几次等参数，生成 TilingData 结构体下发给 kernel。不同的 dtype 组合在该层对应不同的 tiling 策略——`mul_tiling_arch35.cpp` 中维护了一张包含 16 种 dtype 组合的映射表，每种组合指向各自的策略函数。这意味着使用不同的数据类型进行测试，可以触发该映射表中的不同条目，从而覆盖不同的 tiling 代码路径。

### 3.3 op_kernel 层

op_host 完成准备后，计算任务被下发到 NPU AI Core 上执行。op_kernel 层是真正执行乘法运算的地方。

由于不同数据类型的计算需求差异很大，`mul_apt.cpp` 在编译期通过 `if constexpr` 分支，将 8 种数据类型路由到 `mul_dag.h` 中各自对应的计算流水线（DAG）。以下为 8 条计算路径的概览：

|计算路径|适用类型|处理方式|
|---|---|---|
|MulOp|float32, int32, int64, int16, complex64|直接调用硬件乘法指令|
|MulXfp16Op|float16, bfloat16|提升至 float32 → 乘法 → 转回原类型|
|MulInt8Op|int8|提升至 int32 → 乘法 → 饱和截断回 int8|
|MulUint8Op|uint8|提升至 uint32 → 乘法 → 饱和截断回 uint8|
|MulBoolOp|bool|转为 half → 乘法 → 转回 int8|
|MulDoubleOp|double|手写 IEEE 754 双精度乘法（约 200 行）|
|MulComplex32Op|complex32|提升至 complex64 → 乘法 → 转回|
|MulMixFpOp|float16*float32 等|提升到公共类型 → 乘法|

其中 MulDoubleOp 值得特别关注：由于 NPU 硬件不原生支持 double 类型运算，该流水线通过手动拆解双精度浮点数的符号位、指数位和尾数位，用 32 位整数运算模拟 64 位乘法，包含 NaN、Inf、subnormal 等特殊值的处理逻辑，代码量约 200 行，条件分支超过 15 个。

需要说明的是，在本次实验使用的 CPU 模拟器环境下，op_kernel 层的代码由模拟器内部执行，gcov 无法对其进行插桩，因此 kernel 层的覆盖率不可度量。但理解 kernel 的内部结构仍有助于理解不同 dtype 为何会走不同的计算路径，从而指导 op_api 层和 op_host 层的测试设计。

## 4 支持的数据类型组合

`mul_def.cpp` 中注册了 Mul 算子支持的 16 种合法的 `(x1, x2, output)` 数据类型组合：

|序号|x1|x2|output|类别|
|---|---|---|---|---|
|1|BF16|BF16|BF16|同类型|
|2|BF16|FLOAT|FLOAT|混合类型|
|3|FLOAT|BF16|FLOAT|混合类型|
|4|FLOAT16|FLOAT16|FLOAT16|同类型|
|5|FLOAT16|FLOAT|FLOAT|混合类型|
|6|FLOAT|FLOAT16|FLOAT|混合类型|
|7|FLOAT|FLOAT|FLOAT|同类型|
|8|INT32|INT32|INT32|同类型|
|9|UINT8|UINT8|UINT8|同类型|
|10|INT8|INT8|INT8|同类型|
|11|INT64|INT64|INT64|同类型|
|12|INT16|INT16|INT16|同类型|
|13|COMPLEX32|COMPLEX32|COMPLEX32|同类型|
|14|COMPLEX64|COMPLEX64|COMPLEX64|同类型|
|15|BOOL|BOOL|BOOL|同类型|
|16|DOUBLE|DOUBLE|DOUBLE|同类型|

不在此表中的 dtype 组合（如 UINT32）将在 op_api 层被校验拒绝。

## 5 小结

CANN 算子采用 op_api → op_host → op_kernel 的三层架构。op_api 负责参数校验和调度决策，op_host 负责 shape 推断和 tiling 切分规划，op_kernel 在 NPU 上按 dtype 分发到不同的计算流水线执行实际运算。

在 CPU 模拟器环境下，测试用例能够直接影响覆盖率的是 op_api 层和 op_host 层。op_api 层的 `aclnn_mul.cpp` 包含最多的条件分支，可通过不同的 dtype 组合、API 变体和异常输入来覆盖；op_host 层的 `mul_tiling_arch35.cpp` 包含 16 种 dtype 的 tiling 策略分发，可通过遍历不同数据类型来覆盖。理解这一分层结构是设计有效测试用例的基础。