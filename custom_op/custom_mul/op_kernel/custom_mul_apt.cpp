/**
 * custom_mul_apt.cpp
 */

#include "kernel_operator.h"
#include "arch35/custom_mul_dag.h"
#include "arch35/custom_mul_struct.h"

using namespace AscendC;
using namespace CustomMulDag;

extern "C" __global__ __aicore__ void custom_mul_kernel(
    GM_ADDR x1, GM_ADDR x2, GM_ADDR y,
    GM_ADDR workspace, GM_ADDR tiling)
{
    uint32_t blockLen  = *(reinterpret_cast<uint32_t*>(tiling));
    uint32_t totalLen  = *(reinterpret_cast<uint32_t*>(tiling + sizeof(uint32_t)));
    uint32_t blockIdx  = GetBlockIdx();
    uint32_t blockNum  = GetBlockNum();

    CustomMulOp<float> op;
    op.Process(
        reinterpret_cast<__gm__ float*>(x1),
        reinterpret_cast<__gm__ float*>(x2),
        reinterpret_cast<__gm__ float*>(y),
        blockLen, totalLen, blockIdx, blockNum);
}
