#include <iostream>
#include <vector>
#include "acl/acl.h"
#include "aclnnop/aclnn_mul.h"
#define LOG(fmt, ...) printf(fmt "\n", ##__VA_ARGS__)

int64_t GetShapeSize(const std::vector<int64_t>& s) {
  int64_t n = 1; for (auto d : s) n *= d; return n;
}
template <typename T>
int CreateTensor(const std::vector<T>& d, const std::vector<int64_t>& s,
                 void** addr, aclDataType dt, aclTensor** t) {
  auto sz = GetShapeSize(s) * sizeof(T);
  if (aclrtMalloc(addr, sz, ACL_MEM_MALLOC_HUGE_FIRST)) return -1;
  if (aclrtMemcpy(*addr, sz, d.data(), sz, ACL_MEMCPY_HOST_TO_DEVICE)) return -1;
  std::vector<int64_t> st(s.size(), 1);
  for (int64_t i = s.size()-2; i >= 0; i--) st[i] = s[i+1] * st[i+1];
  *t = aclCreateTensor(s.data(), s.size(), dt, st.data(), 0, aclFormat::ACL_FORMAT_ND, s.data(), s.size(), *addr);
  return 0;
}
int main() {
  aclInit(nullptr); aclrtSetDevice(0);
  aclrtStream stream; aclrtCreateStream(&stream);

  // 9-dimensional tensor (beyond MAX_SUPPORT_DIMS_NUMS which is typically 8)
  std::vector<int64_t> sh(9, 2);
  int64_t total = 1; for (auto d : sh) total *= d;  // 512
  std::vector<float> data(total, 1.0f);
  std::vector<float> outD(total, 0.0f);
  void *sA, *oA, *outA;
  aclTensor *sT, *oT, *outT;
  int r1 = CreateTensor(data, sh, &sA, ACL_FLOAT, &sT);
  int r2 = CreateTensor(data, sh, &oA, ACL_FLOAT, &oT);
  int r3 = CreateTensor(outD, sh, &outA, ACL_FLOAT, &outT);

  if (r1 || r2 || r3) {
    LOG("Tensor creation failed (may fail at 9 dims)");
    aclrtDestroyStream(stream); aclrtResetDevice(0); aclFinalize();
    return 0;
  }

  uint64_t ws = 0; aclOpExecutor* ex = nullptr;
  auto st = aclnnMulGetWorkspaceSize(sT, oT, outT, &ws, &ex);

  LOG("=== 4.2_dim_check ===");
  LOG("9-dim tensor (should be REJECTED by MAX_DIM)");
  LOG("GetWorkspaceSize: %d (0=SUCCESS)", st);

  if (st == ACLNN_SUCCESS) {
    LOG("BUG CONFIRMED: 9-dim tensor passed validation");
  } else {
    LOG("No bug: 9-dim tensor correctly rejected (status=%d)", st);
  }

  aclDestroyTensor(sT); aclrtFree(sA);
  aclDestroyTensor(oT); aclrtFree(oA);
  aclDestroyTensor(outT); aclrtFree(outA);
  aclrtDestroyStream(stream); aclrtResetDevice(0); aclFinalize();
  return (st == ACLNN_SUCCESS) ? 1 : 0;
}
