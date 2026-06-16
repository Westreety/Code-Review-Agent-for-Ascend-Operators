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

  std::vector<int64_t> shape = {2, 2};
  std::vector<double> selfData = {1.0, 2.0, 3.0, 4.0};
  std::vector<double> otherData = {2.0, 2.0, 2.0, 2.0};
  std::vector<double> outData(4, 0.0);

  void *selfAddr, *otherAddr, *outAddr;
  aclTensor *self, *other, *out;
  CreateAclTensor(selfData, shape, &selfAddr, ACL_DOUBLE, &self);
  CreateAclTensor(otherData, shape, &otherAddr, ACL_DOUBLE, &other);
  CreateAclTensor(outData, shape, &outAddr, ACL_DOUBLE, &out);

  uint64_t workspaceSize = 0;
  aclOpExecutor* executor = nullptr;
  auto status = aclnnMulGetWorkspaceSize(self, other, out, &workspaceSize, &executor);

  LOG("=== 2.1_mixdtype_removed ===");
  LOG("DT_DOUBLE x DT_DOUBLE (should be legal)");
  LOG("GetWorkspaceSize returned: %d", status);

  if (status != ACLNN_SUCCESS) {
    LOG("BUG CONFIRMED: legal DT_DOUBLE input rejected (status=%d)", status);
  } else {
    LOG("No bug: DT_DOUBLE accepted (status=%d)", status);
  }

  aclDestroyTensor(self); aclrtFree(selfAddr);
  aclDestroyTensor(other); aclrtFree(otherAddr);
  aclDestroyTensor(out); aclrtFree(outAddr);
  aclrtDestroyStream(stream);
  aclrtResetDevice(0);
  aclFinalize();
  return (status != ACLNN_SUCCESS) ? 1 : 0;
}
