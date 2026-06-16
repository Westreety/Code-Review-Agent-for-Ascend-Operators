#include <iostream>
#include <vector>
#include "acl/acl.h"
#include "aclnnop/aclnn_mul.h"

#define LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

int64_t GetShapeSize(const std::vector<int64_t>& shape) {
  int64_t size = 1;
  for (auto dim : shape) size *= dim;
  return size;
}

template <typename T>
int CreateAclTensor(const std::vector<T>& data, const std::vector<int64_t>& shape,
                    void** devAddr, aclDataType dtype, aclTensor** tensor) {
  auto size = GetShapeSize(shape) * sizeof(T);
  if (aclrtMalloc(devAddr, size, ACL_MEM_MALLOC_HUGE_FIRST) != ACL_SUCCESS) return -1;
  if (aclrtMemcpy(*devAddr, size, data.data(), size, ACL_MEMCPY_HOST_TO_DEVICE) != ACL_SUCCESS) return -1;
  std::vector<int64_t> strides(shape.size(), 1);
  for (int64_t i = shape.size()-2; i >= 0; i--) strides[i] = shape[i+1] * strides[i+1];
  *tensor = aclCreateTensor(shape.data(), shape.size(), dtype, strides.data(), 0,
                            aclFormat::ACL_FORMAT_ND, shape.data(), shape.size(), *devAddr);
  return 0;
}

int main() {
  aclInit(nullptr);
  aclrtSetDevice(0);
  aclrtStream stream;
  aclrtCreateStream(&stream);

  std::vector<int64_t> shape = {4, 2};
  std::vector<float> selfData(8, 1.0f);
  std::vector<float> otherData(8, 2.0f);

  void *selfAddr = nullptr, *otherAddr = nullptr;
  aclTensor *self = nullptr, *other = nullptr, *out = nullptr;

  CreateAclTensor(selfData, shape, &selfAddr, ACL_FLOAT, &self);
  CreateAclTensor(otherData, shape, &otherAddr, ACL_FLOAT, &other);

  // === ATTACK: out = nullptr ===
  uint64_t workspaceSize = 0;
  aclOpExecutor* executor = nullptr;
  auto status = aclnnMulGetWorkspaceSize(self, other, out, &workspaceSize, &executor);

  LOG("=== 1.1_nullptr_check ===");
  LOG("GetWorkspaceSize with out=nullptr returned: %d", status);

  if (status == ACLNN_SUCCESS) {
    LOG("BUG CONFIRMED: nullptr out passed validation (status=ACLNN_SUCCESS)");
    // Try to execute - should crash
    void* ws = nullptr;
    if (workspaceSize > 0) aclrtMalloc(&ws, workspaceSize, ACL_MEM_MALLOC_HUGE_FIRST);
    // This may crash - that's the point
    status = aclnnMul(ws, workspaceSize, executor, stream);
    LOG("aclnnMul returned: %d", status);
    if (ws) aclrtFree(ws);
  } else if (status == ACLNN_ERR_PARAM_NULLPTR) {
    LOG("No bug: nullptr correctly rejected");
  } else {
    LOG("Unexpected status: %d", status);
  }

  aclDestroyTensor(self); aclrtFree(selfAddr);
  aclDestroyTensor(other); aclrtFree(otherAddr);
  aclrtDestroyStream(stream);
  aclrtResetDevice(0);
  aclFinalize();
  return (status == ACLNN_SUCCESS) ? 1 : 0;
}
