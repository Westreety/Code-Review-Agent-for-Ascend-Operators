# 4.2 维度上限检查缺失

- **错误分类**: 四类·Shape/广播 (4.2)
- **注入层**: op_api
- **注入位置**: `aclnn_mul.cpp` → `CheckMulShape()` L294-295
- **注入难度**: 🟢 易
- **可检测性**: 🟡 中

## 注入内容

删除 `CheckMulShape()` 中对 `self` 和 `other` 的 `OP_CHECK_MAX_DIM` 检查，超多维 tensor 通过校验。

## 注入方式

```cpp
// 原始:
OP_CHECK_MAX_DIM(self, MAX_SUPPORT_DIMS_NUMS, return false);
OP_CHECK_MAX_DIM(other, MAX_SUPPORT_DIMS_NUMS, return false);

// 注入:
// 两行均被删除
```
