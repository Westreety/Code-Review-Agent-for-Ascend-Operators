# 4.3 输出Shape校验缺失

- **错误分类**: 四类·Shape/广播 (4.3)
- **注入层**: op_api
- **注入位置**: `aclnn_mul.cpp` → `CheckMulShape()` L297
- **注入难度**: 🟢 易
- **可检测性**: 🟢 易

## 注入内容

`CheckMulShape()` 中对 `out` tensor 与广播推导 shape 的比对校验被替换为 `(void)out;`，导致 shape 不匹配的非法输出通过校验。

## 注入方式

```cpp
// 原始:
OP_CHECK_SHAPE_NOT_EQUAL_WITH_EXPECTED_SIZE(out, dstShape, return false);

// 注入:
(void)out;
```
