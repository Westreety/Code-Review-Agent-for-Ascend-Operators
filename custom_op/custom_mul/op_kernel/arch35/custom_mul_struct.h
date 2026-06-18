/**
 * custom_mul_struct.h — tiling key 模板参数定义
 */

#ifndef CUSTOM_MUL_STRUCT_H_
#define CUSTOM_MUL_STRUCT_H_

#include "kernel_operator.h"

// 简化的 tiling key: 只用 schMode 一个字段
ASCENDC_TPL_ARGS_DECL(CustomMul, BRC_TEMP_SCH_MODE_KEY_DECL(schMode));

ASCENDC_TPL_SEL(
    ASCENDC_TPL_ARGS_SEL(BRC_TEMP_SCH_MODE_KEY_SEL(schMode))
);

#endif  // CUSTOM_MUL_STRUCT_H_
