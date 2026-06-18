# 1.3 dtype白名单过宽

- **错误分类**: 一类·参数校验 (1.3)
- **注入层**: op_api
- **注入位置**: `aclnn_mul.cpp` → `ASCEND910B_DTYPE_DTYPE_SUPPORT_LIST` L58-61
- **注入难度**: 🟢 易
- **可检测性**: 🟡 中

## 注入内容

在 910B 的 dtype 支持列表中添加 `DT_UINT32`（不支持的 dtype），导致非法输入通过 API 校验。

## 注入方式

```cpp
// 原始:
..., DataType::DT_BOOL, DataType::DT_COMPLEX128,

// 注入:
..., DataType::DT_BOOL, DataType::DT_UINT32, DataType::DT_COMPLEX128,
```
