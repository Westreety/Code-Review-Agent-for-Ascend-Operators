/**
 * CANN Mul 算子测试模板
 *
 * 使用方法:
 *   1. 根据你发现的 bug 修改 test_attack() 函数中的输入参数
 *   2. 编译: bash build.sh your_test.cpp
 *   3. 运行: ./your_test
 */

#include <iostream>
#include <vector>
#include "acl/acl.h"
#include "aclnnop/aclnn_mul.h"

#define LOG_PRINT(message, ...) \
  do { printf(message, ##__VA_ARGS__); } while (0)

int64_t GetShapeSize(const std::vector<int64_t>& shape) {
  int64_t size = 1;
  for (auto dim : shape) size *= dim;
  return size;
}

template <typename T>
int CreateAclTensor(const std::vector<T>& hostData, const std::vector<int64_t>& shape,
                    void** deviceAddr, aclDataType dataType, aclTensor** tensor) {
  auto size = GetShapeSize(shape) * sizeof(T);
  auto ret = aclrtMalloc(deviceAddr, size, ACL_MEM_MALLOC_HUGE_FIRST);
  if (ret != ACL_SUCCESS) { LOG_PRINT("aclrtMalloc failed. ERROR: %d\n", ret); return ret; }
  ret = aclrtMemcpy(*deviceAddr, size, hostData.data(), size, ACL_MEMCPY_HOST_TO_DEVICE);
  if (ret != ACL_SUCCESS) { LOG_PRINT("aclrtMemcpy failed. ERROR: %d\n", ret); return ret; }
  std::vector<int64_t> strides(shape.size(), 1);
  for (int64_t i = shape.size() - 2; i >= 0; i--) strides[i] = shape[i + 1] * strides[i + 1];
  *tensor = aclCreateTensor(shape.data(), shape.size(), dataType, strides.data(), 0,
                            aclFormat::ACL_FORMAT_ND, shape.data(), shape.size(), *deviceAddr);
  return 0;
}

int main() {
  int32_t deviceId = 0;
  aclrtStream stream;
  int passed = 0, failed = 0;

  auto ret = aclInit(nullptr);
  if (ret != ACL_SUCCESS) { LOG_PRINT("aclInit failed\n"); return 1; }
  ret = aclrtSetDevice(deviceId);
  if (ret != ACL_SUCCESS) { LOG_PRINT("aclrtSetDevice failed\n"); return 1; }
  ret = aclrtCreateStream(&stream);
  if (ret != ACL_SUCCESS) { LOG_PRINT("aclrtCreateStream failed\n"); return 1; }

  // ============================================================
  // TODO: 根据你发现的 bug，修改以下输入参数来触发它
  // ============================================================

  {
    std::vector<int64_t> shape = {4, 2};

    // 示例: 正常的 float 输入
    std::vector<float> selfData  = {0, 1, 2, 3, 4, 5, 6, 7};
    std::vector<float> otherData = {1, 1, 1, 2, 2, 2, 3, 3};

    void *selfAddr = nullptr, *otherAddr = nullptr, *outAddr = nullptr;
    aclTensor *self = nullptr, *other = nullptr, *out = nullptr;
    uint64_t workspaceSize = 0;
    aclOpExecutor* executor = nullptr;

    // 创建输入 tensor
    CreateAclTensor(selfData, shape, &selfAddr, ACL_FLOAT, &self);
    CreateAclTensor(otherData, shape, &otherAddr, ACL_FLOAT, &other);

    // TODO: 根据 bug 特征决定 out 如何构造
    // 例如: 测试空指针 → out = nullptr
    //      测试 shape 不匹配 → out shape 设为不同值
    //      测试 dtype → 换用 ACL_DOUBLE 等
    std::vector<float> outData(GetShapeSize(shape), 0);
    CreateAclTensor(outData, shape, &outAddr, ACL_FLOAT, &out);
    // out = nullptr;  // 取消注释以测试空指针场景

    auto status = aclnnMulGetWorkspaceSize(self, other, out, &workspaceSize, &executor);

    LOG_PRINT("[TEST] GetWorkspaceSize returned: %d (workspaceSize=%lu)\n",
              status, workspaceSize);

    if (status == ACLNN_SUCCESS) {
      LOG_PRINT("[PASS] GetWorkspaceSize succeeded\n");
      passed++;
    } else {
      LOG_PRINT("[FAIL] GetWorkspaceSize failed with error %d\n", status);
      failed++;
    }

    // 清理
    if (self) { aclDestroyTensor(self); aclrtFree(selfAddr); }
    if (other) { aclDestroyTensor(other); aclrtFree(otherAddr); }
    if (out && outData.size() > 0) { aclDestroyTensor(out); aclrtFree(outAddr); }
  }

  aclrtDestroyStream(stream);
  aclrtResetDevice(deviceId);
  aclFinalize();

  LOG_PRINT("\n=== Summary: %d passed, %d failed ===\n", passed, failed);
  return (failed > 0) ? 1 : 0;
}
