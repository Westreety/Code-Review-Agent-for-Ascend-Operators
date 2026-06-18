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

  std::vector<int64_t> sh = {2, 2};
  std::vector<double> sd = {1.0, 2.0, 3.0, 4.0};
  std::vector<double> od = {2.0, 2.0, 2.0, 2.0};
  std::vector<double> outD(4, 0.0);
  void *sA, *oA, *outA;
  aclTensor *sT, *oT, *outT;
  CreateTensor(sd, sh, &sA, ACL_DOUBLE, &sT);
  CreateTensor(od, sh, &oA, ACL_DOUBLE, &oT);
  CreateTensor(outD, sh, &outA, ACL_DOUBLE, &outT);

  uint64_t ws = 0; aclOpExecutor* ex = nullptr;
  auto st = aclnnMulGetWorkspaceSize(sT, oT, outT, &ws, &ex);

  LOG("=== 1.2_dtype_whitelist ===");
  LOG("DT_DOUBLE x DT_DOUBLE (should be legal)");
  LOG("GetWorkspaceSize: %d (expected: ACLNN_SUCCESS=0)", st);

  if (st != ACLNN_SUCCESS) {
    LOG("BUG CONFIRMED: legal DT_DOUBLE rejected (status=%d)", st);
  } else {
    LOG("No bug: DT_DOUBLE accepted");
  }

  aclDestroyTensor(sT); aclrtFree(sA);
  aclDestroyTensor(oT); aclrtFree(oA);
  aclDestroyTensor(outT); aclrtFree(outA);
  aclrtDestroyStream(stream); aclrtResetDevice(0); aclFinalize();
  return (st != ACLNN_SUCCESS) ? 1 : 0;
}
