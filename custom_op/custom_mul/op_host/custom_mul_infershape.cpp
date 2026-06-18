/**
 * custom_mul_infershape.cpp — Shape 推导
 */

#include "log/log.h"
#include "infershape_broadcast_util.h"
#include "register/op_impl_registry.h"

namespace ops {

static ge::graphStatus InferShape4Broadcast(gert::InferShapeContext* context) {
    auto inShape1 = context->GetInputShape(0);
    OP_CHECK_NULL_WITH_CONTEXT(context, inShape1);
    auto inShape2 = context->GetInputShape(1);
    OP_CHECK_NULL_WITH_CONTEXT(context, inShape2);
    auto outShape = context->GetOutputShape(0);
    OP_CHECK_NULL_WITH_CONTEXT(context, outShape);

    OP_CHECK_IF((!Ops::Base::BroadcastShape(inShape1, inShape2, outShape)),
                OP_LOGE(context->GetNodeName(), "BroadcastShape failed."),
                return ge::GRAPH_FAILED);

    return ge::GRAPH_SUCCESS;
}

IMPL_OP_INFERSHAPE(CustomMul).InferShape(InferShape4Broadcast);

}  // namespace ops
