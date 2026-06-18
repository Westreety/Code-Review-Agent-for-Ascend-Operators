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

  // self=[2,3], other=[2,3] → broadcast shape = [2,3]
  // But out is given shape=[1,1] (mismatch)
  std::vector<int64_t> inShape = {2, 3};
  std::vector<int64_t> outShape = {1, 1};  // ATTACK: wrong shape
  std::vector<float> selfData(6, 2.0f);
  std::vector<float> otherData(6, 3.0f);
  std::vector<float> outData(1, 0.0f);

  void *selfAddr, *otherAddr, *outAddr;
  aclTensor *self, *other, *out;
  CreateAclTensor(selfData, inShape, &selfAddr, ACL_FLOAT, &self);
  CreateAclTensor(otherData, inShape, &otherAddr, ACL_FLOAT, &other);
  CreateAclTensor(outData, outShape, &outAddr, ACL_FLOAT, &out);

  uint64_t workspaceSize = 0;
  aclOpExecutor* executor = nullptr;
  auto status = aclnnMulGetWorkspaceSize(self, other, out, &workspaceSize, &executor);

  LOG("=== 4.3_shape_verify ===");
  LOG("self=[2,3] other=[2,3] out=[1,1] shape mismatch");
  LOG("GetWorkspaceSize returned: %d", status);

  if (status == ACLNN_SUCCESS) {
    LOG("BUG CONFIRMED: shape mismatch passed validation");
  } else {
    LOG("No bug: shape mismatch correctly rejected (status=%d)", status);
  }

  aclDestroyTensor(self); aclrtFree(selfAddr);
  aclDestroyTensor(other); aclrtFree(otherAddr);
  aclDestroyTensor(out); aclrtFree(outAddr);
  aclrtDestroyStream(stream);
  aclrtResetDevice(0);
  aclFinalize();
  return (status == ACLNN_SUCCESS) ? 1 : 0;
}
