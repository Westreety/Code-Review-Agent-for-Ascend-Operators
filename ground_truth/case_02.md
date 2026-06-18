# Ground Truth: case_02_output_shape

## Bug 信息

| 项目 | 内容 |
|------|------|
| **文件** | `math/mul/op_api/aclnn_mul.cpp` |
| **函数** | `CheckMulShape()` |
| **行号** | 第 297 行 |
| **错误分类** | 四类·Shape 检查 (4.3) — 输出 Shape 校验缺失 |
| **注入方式** | `OP_CHECK_SHAPE_NOT_EQUAL_WITH_EXPECTED_SIZE(out, dstShape, return false)` → `(void)out;` |

## Bug 描述

`CheckMulShape()` 中删除了输出 tensor 的 shape 一致性检查。广播推导出 `dstShape=[2,3]` 之后，不再验证 `out` 的实际 shape 是否与之匹配。用户传入错误 shape 的 `out` 时，校验通过，后续算子会向该 `out` 写入 `[2,3]` 的数据——如果 `out` 实际只有 `[6]`，则发生越界写入。

## 正确代码

```cpp
inline static bool CheckMulShape(const aclTensor *self, const aclTensor *other, const aclTensor *out) {
  Shape dstShape;
  OP_CHECK_MAX_DIM(self, MAX_SUPPORT_DIMS_NUMS, return false);
  OP_CHECK_MAX_DIM(other, MAX_SUPPORT_DIMS_NUMS, return false);
  OP_CHECK_BROADCAST_AND_INFER_SHAPE(self, other, dstShape, return false);
  OP_CHECK_SHAPE_NOT_EQUAL_WITH_EXPECTED_SIZE(out, dstShape, return false);  // ← 这行被删了
  return true;
}
```

## 触发方式

`self=[2,3]` × `other=[2,3]`，广播后应为 `[2,3]`，但 `out` 声明为 `[6]`。

## 预期结果

- 正确算子：返回错误码（如 `ACLNN_ERR_PARAM_INVALID = 161002`）
- Buggy 算子：返回非 SUCCESS 错误码（如 561103），或越界写入

## 评分建议

| 维度 | 2 分标准 |
|------|------|
| bug 发现 | 定位到 `CheckMulShape()` 第 297 行，指出缺少输出 shape 校验 |
| 测试生成 | 测试代码构造不匹配的 shape（如 `[2,3]` vs `[6]`） |
| 解释修复 | 恢复 `OP_CHECK_SHAPE_NOT_EQUAL_WITH_EXPECTED_SIZE` |
