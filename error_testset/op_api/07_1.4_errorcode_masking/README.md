# 1.4 错误码伪装

- **错误分类**: 一类·参数校验 (1.4)
- **注入层**: op_api
- **注入位置**: `aclnn_mul.cpp` → `CheckMulParams()` L322
- **注入难度**: 🟢 易
- **可检测性**: 🟢 易

## 注入内容

`CheckMulParams` 中空指针校验失败时返回 `ACLNN_SUCCESS` 而非 `ACLNN_ERR_PARAM_NULLPTR`，错误被吞噬，后续解引用崩溃。

## 注入方式

```cpp
// 原始:
CHECK_RET(CheckMulNotNull(self, other, out), ACLNN_ERR_PARAM_NULLPTR);

// 注入:
CHECK_RET(CheckMulNotNull(self, other, out), ACLNN_SUCCESS);
```
