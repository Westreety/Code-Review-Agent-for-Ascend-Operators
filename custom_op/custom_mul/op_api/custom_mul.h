/**
 * custom_mul.h — L0 层头文件
 */

#ifndef OP_API_INC_LEVEL0_CUSTOM_MUL_H_
#define OP_API_INC_LEVEL0_CUSTOM_MUL_H_

#include "opdev/op_executor.h"

namespace l0op {

const aclTensor* CustomMul(const aclTensor* self, const aclTensor* other, aclOpExecutor* executor);

}  // namespace l0op

#endif  // OP_API_INC_LEVEL0_CUSTOM_MUL_H_
