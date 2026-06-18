# Ground Truth: case_01_nullptr_check

## Bug 信息

| 项目 | 内容 |
|------|------|
| **文件** | `math/mul/op_api/aclnn_mul.cpp` |
| **函数** | `CheckMulNotNull()` |
| **行号** | 第 143 行 |
| **错误分类** | 一类·参数校验 (1.1) — 空指针校验缺失 |
| **注入方式** | `OP_CHECK_NULL(out, return false)` → `(void)out;` |

## Bug 描述

`CheckMulNotNull()` 函数对 `out` 参数的空指针检查被删除。当调用者传入 `out = nullptr` 时，函数仍然返回 `true`（校验通过），后续 `aclnnMulGetWorkspaceSize` 会对 `out` 解引用导致段错误。

## 正确代码

```cpp
inline static bool CheckMulNotNull(const aclTensor *self, const aclTensor *other, const aclTensor *out) {
  OP_CHECK_NULL(self, return false);
  OP_CHECK_NULL(other, return false);
  OP_CHECK_NULL(out, return false);   // ← 这行被删了
  return true;
}
```

## 触发方式

构造 `self` 和 `other` 为合法 tensor（`shape=[4,2], dtype=FLOAT`），`out` 传 `nullptr`。调用 `aclnnMulGetWorkspaceSize(self, other, nullptr, &workspaceSize, &executor)`。

## 预期结果

- 正确算子：返回 `ACLNN_ERR_PARAM_NULLPTR (161001)`
- Buggy 算子：段错误 (SEGFAULT, exit=139)

## 评分建议

| 维度 | 2 分标准 |
|------|------|
| bug 发现 | 定位到 `CheckMulNotNull()` 第 143 行，指出缺少 `out` 的空指针检查 |
| 测试生成 | 测试代码传 `nullptr` 作为 `out` 参数 |
| 解释修复 | 恢复 `OP_CHECK_NULL(out, return false)` |
