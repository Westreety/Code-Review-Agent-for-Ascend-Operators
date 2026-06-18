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
  std::vector<uint32_t> sd = {1, 2, 3, 4};
  std::vector<uint32_t> od = {2, 2, 2, 2};
  std::vector<uint32_t> outD(4, 0);
  void *sA, *oA, *outA;
  aclTensor *sT, *oT, *outT;
  CreateTensor(sd, sh, &sA, ACL_UINT32, &sT);
  CreateTensor(od, sh, &oA, ACL_UINT32, &oT);
  CreateTensor(outD, sh, &outA, ACL_UINT32, &outT);

  uint64_t ws = 0; aclOpExecutor* ex = nullptr;
  auto st = aclnnMulGetWorkspaceSize(sT, oT, outT, &ws, &ex);

  LOG("=== 1.3_dtype_overwide ===");
  LOG("DT_UINT32 x DT_UINT32 (should be REJECTED)");
  LOG("GetWorkspaceSize: %d", st);

  if (st == ACLNN_SUCCESS) {
    LOG("BUG CONFIRMED: illegal DT_UINT32 passed validation");
  } else {
    LOG("No bug: DT_UINT32 correctly rejected (status=%d)", st);
  }

  aclDestroyTensor(sT); aclrtFree(sA);
  aclDestroyTensor(oT); aclrtFree(oA);
  aclDestroyTensor(outT); aclrtFree(outA);
  aclrtDestroyStream(stream); aclrtResetDevice(0); aclFinalize();
  return (st == ACLNN_SUCCESS) ? 1 : 0;
}
