# 1.1 空指针校验缺失

- **错误分类**: 一类·参数校验 (1.1)
- **注入层**: op_api
- **注入位置**: `aclnn_mul.cpp` → `CheckMulNotNull()` L143
- **注入难度**: 🟢 易
- **可检测性**: 🟢 易

## 注入内容

`CheckMulNotNull()` 中 `out` 参数的空指针检查被替换为 `(void)out;`，导致调用方传入 `out=nullptr` 时校验通过，后续 `out->GetDataType()` 解引用导致 SEGFAULT。

## 注入方式

```cpp
// 原始:
OP_CHECK_NULL(out, return false);

// 注入:
(void)out;
```

## 验证方法

```bash
bash run.sh
```
