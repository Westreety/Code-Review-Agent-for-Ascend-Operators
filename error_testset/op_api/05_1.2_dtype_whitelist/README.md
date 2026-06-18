# 1.2 dtype白名单遗漏

- **错误分类**: 一类·参数校验 (1.2)
- **注入层**: op_api
- **注入位置**: `aclnn_mul.cpp` → `ASCEND910B_DTYPE_DTYPE_SUPPORT_LIST` L58-61
- **注入难度**: 🟢 易
- **可检测性**: 🟢 易

## 注入内容

从 910B 的 dtype 支持列表中删除 `DT_DOUBLE`，导致合法的 DOUBLE 类型输入在 API 层被拒绝。

## 注入方式

```cpp
// 原始:
DataType::DT_FLOAT, ..., DataType::DT_DOUBLE, DataType::DT_INT8,

// 注入:
DataType::DT_FLOAT, ..., DataType::DT_INT8,  // DT_DOUBLE 被删除
```
