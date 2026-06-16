/**
 * custom_mul.cpp — L0 层实现
 *
 * 直接路由到 AiCore，不做 AiCpu fallback（简化）。
 */

#include "custom_mul.h"
#include "opdev/make_op_executor.h"
#include "opdev/op_dfx.h"
#include "opdev/op_def.h"
#include "opdev/op_log.h"
#include "opdev/op_executor.h"
#include "opdev/shape_utils.h"
#include "op_api/aclnn_check.h"

using namespace op;

namespace l0op {

OP_TYPE_REGISTER(CustomMul);

static const aclTensor* CustomMulAiCore(
    const aclTensor* self, const aclTensor* other,
    const aclTensor* mulOut, aclOpExecutor* executor)
{
    L0_DFX(CustomMulAiCore, self, other, mulOut);
    ADD_TO_LAUNCHER_LIST_AICORE(CustomMul, OP_INPUT(self, other), OP_OUTPUT(mulOut));
    return mulOut;
}

const aclTensor* CustomMul(const aclTensor* self, const aclTensor* other, aclOpExecutor* executor)
{
    Shape broadcastShape;
    OP_CHECK_BROADCAST_AND_INFER_SHAPE(self, other, broadcastShape, return nullptr);

    auto mulOut = executor->AllocTensor(broadcastShape, self->GetDataType());
    return CustomMulAiCore(self, other, mulOut, executor);
}

}  // namespace l0op
