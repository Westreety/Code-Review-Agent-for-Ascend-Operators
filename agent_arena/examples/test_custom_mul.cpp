/**
 * test_custom_mul.cpp — CustomMul 算子的 L0 层调用示例
 *
 * 使用方式：
 *   g++ -std=c++17 -D_GLIBCXX_USE_CXX11_ABI=0 test_custom_mul.cpp \
 *     -I/usr/local/Ascend/cann-8.5.0/include \
 *     -I/usr/local/Ascend/cann-8.5.0/aarch64-linux/include \
 *     -I/usr/local/Ascend/cann-8.5.0/aarch64-linux/include/aclnn \
 *     -I/usr/local/Ascend/cann-8.5.0/aarch64-linux/pkg_inc \
 *     -L/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64 \
 *     -L/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_test_math/op_api/lib \
 *     -lascendcl -lnnopbase -lcust_opapi -lunified_dlog \
 *     -Wl,-rpath,/usr/local/Ascend/ascend-toolkit/latest/aarch64-linux/lib64 \
 *     -Wl,-rpath,/usr/local/Ascend/cann-8.5.0/opp/vendors/custom_test_math/op_api/lib \
 *     -o test_custom_mul
 */

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <vector>
#include "acl/acl.h"
#include "aclnn/opdev/op_executor.h"
#include "aclnn/opdev/make_op_executor.h"
#include "aclnn/opdev/shape_utils.h"

// CustomMul 算子的 L0 层接口（来自 custom_op/custom_mul/op_api/custom_mul.h）
namespace l0op {
const aclTensor* CustomMul(const aclTensor* self, const aclTensor* other, aclOpExecutor* executor);
}

#define LOG_PRINT(message, ...) do { printf(message, ##__VA_ARGS__); } while (0)

int64_t GetShapeSize(const std::vector<int64_t>& shape) {
  int64_t size = 1;
  for (auto i : shape) size *= i;
  return size;
}

template <typename T>
int CreateAclTensor(const std::vector<T>& hostData, const std::vector<int64_t>& shape,
                    void** deviceAddr, aclDataType dataType, aclTensor** tensor) {
  auto size = GetShapeSize(shape) * sizeof(T);
  auto ret = aclrtMalloc(deviceAddr, size, ACL_MEM_MALLOC_HUGE_FIRST);
  if (ret != 0) { LOG_PRINT("aclrtMalloc failed: %d\n", ret); return ret; }
  ret = aclrtMemcpy(*deviceAddr, size, hostData.data(), size, ACL_MEMCPY_HOST_TO_DEVICE);
  if (ret != 0) { LOG_PRINT("aclrtMemcpy failed: %d\n", ret); return ret; }
  std::vector<int64_t> strides(shape.size(), 1);
  for (int64_t i = shape.size() - 2; i >= 0; i--) strides[i] = shape[i + 1] * strides[i + 1];
  *tensor = aclCreateTensor(shape.data(), shape.size(), dataType, strides.data(), 0,
                            aclFormat::ACL_FORMAT_ND, shape.data(), shape.size(), *deviceAddr);
  return 0;
}

int main() {
  aclInit(nullptr);
  aclrtSetDevice(0);

  std::vector<int64_t> shape = {4};
  std::vector<float> selfData  = {1.0f, 2.0f, 3.0f, 4.0f};
  std::vector<float> otherData = {2.0f, 2.0f, 2.0f, 2.0f};
  std::vector<float> expected  = {2.0f, 4.0f, 6.0f, 8.0f};

  void *selfAddr = nullptr, *otherAddr = nullptr;
  aclTensor *self = nullptr, *other = nullptr;

  CreateAclTensor(selfData, shape, &selfAddr, ACL_FLOAT, &self);
  CreateAclTensor(otherData, shape, &otherAddr, ACL_FLOAT, &other);

  auto uniqueExecutor = CREATE_EXECUTOR();
  auto* result = l0op::CustomMul(self, other, uniqueExecutor.get());

  if (result == nullptr) {
    LOG_PRINT("CustomMul returned nullptr\n");
    return 1;
  }

  uint64_t ws = uniqueExecutor->GetWorkspaceSize();
  void* workspace = nullptr;
  if (ws > 0) aclrtMalloc(&workspace, ws, ACL_MEM_MALLOC_HUGE_FIRST);

  aclrtStream stream;
  aclrtCreateStream(&stream);
  CommonOpExecutorRun(workspace, ws, uniqueExecutor.get(), stream);
  aclrtSynchronizeStream(stream);

  auto outShape = result->GetViewShape();
  uint64_t outSize = 1;
  for (int64_t i = 0; i < outShape.GetDimNum(); i++) outSize *= outShape.GetDim(i);

  std::vector<float> outData(outSize);
  aclrtMemcpy(outData.data(), outSize * sizeof(float),
              result->GetData(), outSize * sizeof(float), ACL_MEMCPY_DEVICE_TO_HOST);

  LOG_PRINT("Input:  [%.1f, %.1f, %.1f, %.1f]\n", selfData[0], selfData[1], selfData[2], selfData[3]);
  LOG_PRINT("Output: [%.1f, %.1f, %.1f, %.1f]\n", outData[0], outData[1], outData[2], outData[3]);
  LOG_PRINT("Expect: [%.1f, %.1f, %.1f, %.1f]\n", expected[0], expected[1], expected[2], expected[3]);

  bool ok = true;
  for (size_t i = 0; i < outSize; i++) {
    if (outData[i] != expected[i]) { ok = false; break; }
  }
  LOG_PRINT("Result: %s\n", ok ? "PASS" : "FAIL");

  if (workspace) aclrtFree(workspace);
  aclDestroyTensor(self); aclrtFree(selfAddr);
  aclDestroyTensor(other); aclrtFree(otherAddr);
  aclrtDestroyStream(stream);
  aclrtResetDevice(0);
  aclFinalize();
  return ok ? 0 : 1;
}
