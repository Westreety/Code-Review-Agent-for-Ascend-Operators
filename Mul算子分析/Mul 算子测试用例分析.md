## 1 引言

本文分析 Mul 算子的已有测试用例，帮助读者理解端到端算子测试的编写方法和设计思路。

ops-math 仓库中，每个算子的测试代码分为两类：`tests/ut/` 下的单元测试（UT）使用 Google Test 框架，通过 Faker/Stub 机制在纯 CPU 环境下分别测试各层的内部逻辑（如 API 层的参数校验、Host 层的 shape 推断和 tiling 策略），但不执行算子的实际计算，也不验证数值结果。`examples/` 下的功能示例则通过 aclnn API 驱动算子在真实设备或 CPU 模拟器上完成端到端的执行，能够验证最终的计算输出。本次比赛中只考虑 `example` 中的算子测试用例开发。

本次实验基于 CPU 模拟器环境，目标是验证算子的计算正确性，因此采用 examples 风格的端到端测试作为基础。下面以 `math/mul/examples/test_aclnn_mul.cpp` 这一官方示例为对象，对其代码进行逐段解析。

## 2 代码全貌

`math/mul/examples/test_aclnn_mul.cpp` 的完整代码共 147 行，可分为四个部分：工具宏与辅助函数（第 1-68 行）、运行时初始化（第 70-76 行）、数据准备与算子执行（第 78-119 行）、结果输出与资源释放（第 121-147 行）。以下逐一分析。

## 3 工具宏与辅助函数

文件开头引入了两个头文件和两个工具宏：

```cpp
#include "acl/acl.h"          // ACL 运行时接口（设备管理、内存管理、流管理）
#include "aclnnop/aclnn_mul.h" // Mul 算子的 aclnn 接口
```

`acl/acl.h` 提供了设备初始化、内存分配、数据拷贝等运行时基础能力。`aclnnop/aclnn_mul.h` 声明了 Mul 算子的四个 API 函数（aclnnMul、aclnnMuls、aclnnInplaceMul、aclnnInplaceMuls），每个 API 包含 GetWorkspaceSize 和 Execute 两个阶段的函数。

```cpp
#define CHECK_RET(cond, return_expr) \
  do { if (!(cond)) { return_expr; } } while (0)

#define LOG_PRINT(message, ...) \
  do { printf(message, ##__VA_ARGS__); } while (0)
```

`CHECK_RET` 是一个错误检查宏：当条件 `cond` 不满足时，执行 `return_expr`（通常是打印错误信息并返回）。该宏在后续代码中被大量使用，用于简化错误处理流程。

接下来是两个辅助函数。`GetShapeSize` 计算 shape 中所有维度的乘积，即 tensor 中的元素总数：

```cpp
int64_t GetShapeSize(const std::vector<int64_t>& shape) {
  int64_t shapeSize = 1;
  for (auto i : shape) {
    shapeSize *= i;
  }
  return shapeSize;
}
```

例如 shape 为 `{4, 2}` 时返回 8。

`Init` 函数封装了 ACL 运行时的初始化流程：

```cpp
int Init(int32_t deviceId, aclrtStream* stream) {
  auto ret = aclInit(nullptr);           // 初始化 ACL 框架
  CHECK_RET(ret == ACL_SUCCESS, ...);
  ret = aclrtSetDevice(deviceId);        // 选择计算设备
  CHECK_RET(ret == ACL_SUCCESS, ...);
  ret = aclrtCreateStream(stream);       // 创建任务流
  CHECK_RET(ret == ACL_SUCCESS, ...);
  return 0;
}
```

`aclInit` 初始化整个 ACL 运行时环境，`aclrtSetDevice` 指定使用哪个设备（在 CPU 模拟器下通常为 0），`aclrtCreateStream` 创建一个计算流用于提交异步任务。这三步是所有 ACL 程序的固定开头。

`CreateAclTensor` 是最关键的辅助函数，完成从主机数据到设备 tensor 的全部准备工作：

```cpp
template <typename T>
int CreateAclTensor(const std::vector<T>& hostData,
                    const std::vector<int64_t>& shape,
                    void** deviceAddr,
                    aclDataType dataType,
                    aclTensor** tensor) {
  auto size = GetShapeSize(shape) * sizeof(T);
  // (1) 在设备侧分配内存
  auto ret = aclrtMalloc(deviceAddr, size, ACL_MEM_MALLOC_HUGE_FIRST);
  CHECK_RET(ret == ACL_SUCCESS, ...);
  // (2) 将主机数据拷贝到设备内存
  ret = aclrtMemcpy(*deviceAddr, size, hostData.data(), size,
                    ACL_MEMCPY_HOST_TO_DEVICE);
  CHECK_RET(ret == ACL_SUCCESS, ...);
  // (3) 计算行优先存储的步长 strides
  std::vector<int64_t> strides(shape.size(), 1);
  for (int64_t i = shape.size() - 2; i >= 0; i--) {
    strides[i] = shape[i + 1] * strides[i + 1];
  }
  // (4) 创建 aclTensor 描述符
  *tensor = aclCreateTensor(shape.data(), shape.size(), dataType,
                            strides.data(), 0, aclFormat::ACL_FORMAT_ND,
                            shape.data(), shape.size(), *deviceAddr);
  return 0;
}
```

这个函数做了四件事。第一步通过 `aclrtMalloc` 在设备（NPU 或模拟器）上分配一块与数据等大的内存。第二步通过 `aclrtMemcpy` 将 CPU 上准备好的数据拷贝到设备内存。第三步计算 strides——对于 shape 为 `{4, 2}` 的 tensor，strides 为 `{2, 1}`，表示第 0 维相邻元素间隔 2 个元素，第 1 维相邻元素间隔 1 个元素。第四步调用 `aclCreateTensor` 创建描述符，该函数的参数依次为：shape 数组、维度数、数据类型、strides 数组、offset、数据格式、storage shape 数组、storage shape 维度数、设备内存地址。参数顺序和数量必须严格对应，这是上一轮实验中学生出错频率最高的 API。

## 4 主函数：数据准备

`main` 函数的前半部分完成初始化和数据准备：

```cpp
int main() {
  // 初始化设备和 stream
  int32_t deviceId = 0;
  aclrtStream stream;
  auto ret = Init(deviceId, &stream);
  CHECK_RET(ret == ACL_SUCCESS, ...);

  // 定义输入输出的 shape 和数据
  std::vector<int64_t> selfShape = {4, 2};
  std::vector<int64_t> otherShape = {4, 2};
  std::vector<int64_t> outShape = {4, 2};
  void* selfDeviceAddr = nullptr;
  void* otherDeviceAddr = nullptr;
  void* outDeviceAddr = nullptr;
  aclTensor* self = nullptr;
  aclTensor* other = nullptr;
  aclTensor* out = nullptr;
  std::vector<float> selfHostData = {0, 1, 2, 3, 4, 5, 6, 7};
  std::vector<float> otherHostData = {1, 1, 1, 2, 2, 2, 3, 3};
  std::vector<float> outHostData(8, 0);
  // 分别创建三个 tensor
  ret = CreateAclTensor(selfHostData, selfShape, &selfDeviceAddr,
                        aclDataType::ACL_FLOAT, &self);
  ret = CreateAclTensor(otherHostData, otherShape, &otherDeviceAddr,
                        aclDataType::ACL_FLOAT, &other);
  ret = CreateAclTensor(outHostData, outShape, &outDeviceAddr,
                        aclDataType::ACL_FLOAT, &out);
```

这里构造了一个具体的测试场景：两个 shape 为 `{4, 2}` 的 float32 tensor 相乘。`self` 的值为 `{0, 1, 2, 3, 4, 5, 6, 7}`，`other` 的值为 `{1, 1, 1, 2, 2, 2, 3, 3}`，预期输出为 `{0*1, 1*1, 2*1, 3*2, 4*2, 5*2, 6*3, 7*3}` = `{0, 1, 2, 6, 8, 10, 18, 21}`。`out` 的主机数据初始化为全零，作为输出的占位。

注意三个 tensor 需要分别分配各自的设备内存和描述符，共六个变量（三个 `void*` 设备地址 + 三个 `aclTensor*` 描述符），在最后都需要逐一释放。

## 5 主函数：算子执行

数据准备完成后，进入 CANN 算子的两段式调用：

```cpp
  // 第一段：获取 workspace 大小和执行器
  uint64_t workspaceSize = 0;
  aclOpExecutor* executor;
  ret = aclnnMulGetWorkspaceSize(self, other, out,
                                  &workspaceSize, &executor);
  CHECK_RET(ret == ACL_SUCCESS, ...);

  // 根据返回的 workspaceSize 分配临时内存
  void* workspaceAddr = nullptr;
  if (workspaceSize > 0) {
    ret = aclrtMalloc(&workspaceAddr, workspaceSize,
                      ACL_MEM_MALLOC_HUGE_FIRST);
    CHECK_RET(ret == ACL_SUCCESS, ...);
  }

  // 第二段：执行算子
  ret = aclnnMul(workspaceAddr, workspaceSize, executor, stream);
  CHECK_RET(ret == ACL_SUCCESS, ...);

  // 同步等待执行完成
  ret = aclrtSynchronizeStream(stream);
  CHECK_RET(ret == ACL_SUCCESS, ...);
```

两段式调用是 CANN aclnn 接口的核心模式。第一段 `aclnnMulGetWorkspaceSize` 接收输入和输出的 tensor 描述符，在内部完成参数校验、类型推导和执行计划的生成，返回两个结果：`workspaceSize`（算子执行所需的临时内存大小）和 `executor`（封装了计算流程的执行器）。如果参数不合法（如空指针、不支持的 dtype），该函数会返回非 `ACL_SUCCESS` 的错误码。

用户根据返回的 `workspaceSize` 分配设备内存后，调用第二段 `aclnnMul` 提交计算任务。注意第二段的参数是 `(workspace, workspaceSize, executor, stream)`，而非 tensor——tensor 信息已经在第一段被封装进了 executor 中。

`aclnnMul` 是异步执行的，调用返回时计算可能尚未完成，因此必须调用 `aclrtSynchronizeStream` 等待 stream 上的所有任务执行结束后，才能读取结果。

## 6 主函数：结果输出与资源释放

```cpp
  // 将结果从设备拷回主机
  auto size = GetShapeSize(outShape);
  std::vector<float> resultData(size, 0);
  ret = aclrtMemcpy(resultData.data(),
                    resultData.size() * sizeof(resultData[0]),
                    outDeviceAddr,
                    size * sizeof(resultData[0]),
                    ACL_MEMCPY_DEVICE_TO_HOST);
  CHECK_RET(ret == ACL_SUCCESS, ...);

  // 打印结果
  for (int64_t i = 0; i < size; i++) {
    LOG_PRINT("result[%ld] is: %f\n", i, resultData[i]);
  }

  // 释放 tensor 描述符
  aclDestroyTensor(self);
  aclDestroyTensor(other);
  aclDestroyTensor(out);

  // 释放设备内存
  aclrtFree(selfDeviceAddr);
  aclrtFree(otherDeviceAddr);
  aclrtFree(outDeviceAddr);
  if (workspaceSize > 0) {
    aclrtFree(workspaceAddr);
  }

  // 销毁 stream、重置设备、反初始化
  aclrtDestroyStream(stream);
  aclrtResetDevice(deviceId);
  aclFinalize();
  return 0;
}
```

`aclrtMemcpy` 使用 `ACL_MEMCPY_DEVICE_TO_HOST` 方向标志将计算结果从设备内存拷回主机端的 `resultData` 向量。随后通过循环打印每个元素的值。

资源释放部分需要逐一清理所有分配的对象：先销毁 `aclTensor` 描述符（`aclDestroyTensor`），再释放设备内存（`aclrtFree`），最后销毁 stream、重置设备并反初始化 ACL 环境。

## 7 该示例的不足

分析完整个代码可以看到，该官方示例存在一个关键不足：**它只打印了计算结果，但没有将结果与期望值进行比对**。也就是说，即使算子将 `2 * 3` 算成了 `7`，程序仍然会正常打印 `result[2] is: 7.000000` 并返回 0（成功）。这样的代码只能确认算子"能运行"，无法确认算子"算得对"。

要将这个示例改造为有效的测试用例，需要补充结果验证逻辑。核心思路是在 CPU 端用高精度（double）独立计算出每个元素的期望值，然后与算子输出逐元素比对。对于浮点类型，比对时需要使用容差而非精确相等，因为浮点运算本身存在精度限制。通用的比对公式为：

∣actual−expected∣≤atol+rtol×∣expected∣|actual - expected| \leq atol + rtol \times |expected|∣actual−expected∣≤atol+rtol×∣expected∣

其中 `atol` 为绝对容差，`rtol` 为相对容差。不同数据类型应使用不同的容差——float32 约有 7 位有效数字（建议 atol=rtol=1e-5），float16 约有 3 位（建议 atol=rtol=1e-3），整数类型则应精确匹配。

除了结果验证之外，该示例的覆盖面也很有限：仅测试了一种数据类型（float32）、一种 shape（{4,2}）、一组正常范围的数值、一个 API（aclnnMul）。如《Mul 算子架构分析》中所述，Mul 算子支持 16 种 dtype 组合，`aclnn_mul.cpp` 中包含约 20 个条件分支用于处理不同的类型组合和调度路径，`mul_tiling_arch35.cpp` 中维护了 16 种 dtype 的 tiling 策略映射表，还有 Muls 和 InplaceMul 等 API 变体走不同的调度逻辑。要有效覆盖算子的各种行为，需要在这个骨架基础上增加更多维度的测试用例——包括不同的数据类型（触发不同的 tiling 策略和类型提升逻辑）、不同的 shape（含广播场景，覆盖 infershape 和不同的 tiling 分支）、数值边界条件（零、负数、极大值、NaN、Inf、整数溢出等）、不同的 API 变体（Mul、Muls、InplaceMul、InplaceMuls 在 op_api 层走不同的代码路径）、以及异常输入（nullptr、不支持的 dtype 等，覆盖参数校验分支）。


