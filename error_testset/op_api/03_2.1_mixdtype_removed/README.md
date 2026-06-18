# 2.1 混合精度组合被误删 (DT_DOUBLE硬编码拒绝)

- **错误分类**: 二类·类型推导 (2.1)
- **注入层**: op_api
- **注入位置**: `aclnn_mul.cpp` → `aclnnMulGetWorkspaceSize()` L466
- **注入难度**: 🟡 中
- **可检测性**: 🟡 中
- **⚠️ 注**: 此注入为 DT_DOUBLE 硬编码拒绝，与 2.1 原始定义(BF16-FLOAT优化路径移除)有差异

## 注入内容

在 `aclnnMulGetWorkspaceSize()` 中插入：
```cpp
if (self->GetDataType() == DataType::DT_DOUBLE) return ACLNN_ERR_PARAM_INVALID;
```

DT_DOUBLE 在 support list 中且通过了前置 dtype 校验，但在 workspace 计算时被硬编码拒绝，造成 API 契约不一致。
