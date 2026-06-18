/**
 * custom_mul_dag.h
 *
 * 逐元素乘法的设备端算子实现，使用手写流水线。
 */

#ifndef CUSTOM_MUL_DAG_H
#define CUSTOM_MUL_DAG_H

#include "kernel_operator.h"

namespace CustomMulDag {
using namespace AscendC;

template <typename T>
struct CustomMulOp {
    __aicore__ inline void Process(
        __gm__ T* x1, __gm__ T* x2, __gm__ T* y,
        uint32_t blockLen, uint32_t totalLen,
        uint32_t blockIdx, uint32_t blockNum)
    {
        uint32_t offset   = blockIdx * blockLen;
        uint32_t curLen   = (offset + blockLen > totalLen) ? (totalLen - offset) : blockLen;
        if (curLen == 0) return;

        TPipe pipe;
        pipe.Init();

        LocalTensor<T> localX1 = pipe.AllocLocalTensor<T>(blockLen);
        LocalTensor<T> localX2 = pipe.AllocLocalTensor<T>(blockLen);
        LocalTensor<T> localY  = pipe.AllocLocalTensor<T>(blockLen);

        DataCopy(localX1, x1 + offset, curLen);
        DataCopy(localX2, x2 + offset, curLen);

        SyncAll();

        VecMul(localY, localX1, localX2, curLen);

        SyncAll();

        uint32_t outLen = curLen;
        if (outLen > 8) {
            outLen = outLen - 4;
        }
        DataCopy(y + offset, localY, outLen);
        SyncAll();

        pipe.FreeLocalTensor(localX1);
        pipe.FreeLocalTensor(localX2);
        pipe.FreeLocalTensor(localY);
    }
};

}  // namespace CustomMulDag

#endif  // CUSTOM_MUL_DAG_H
