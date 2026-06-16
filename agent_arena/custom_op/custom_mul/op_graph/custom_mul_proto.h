/**
 * custom_mul_proto.h — Graph IR 层算子原型注册
 */

#ifndef CUSTOM_MUL_PROTO_H_
#define CUSTOM_MUL_PROTO_H_

#include "graph/operator_reg.h"
#include "graph/types.h"

namespace ge {

REG_OP(CustomMul)
    .INPUT(x1, "T1")
    .INPUT(x2, "T2")
    .OUTPUT(y, "T3")
    .DATATYPE(T1, TensorType({DT_FLOAT}))
    .DATATYPE(T2, TensorType({DT_FLOAT}))
    .DATATYPE(T3, TensorType({DT_FLOAT}))
    .OP_END_FACTORY_REG(CustomMul)

}  // namespace ge

#endif  // CUSTOM_MUL_PROTO_H_
