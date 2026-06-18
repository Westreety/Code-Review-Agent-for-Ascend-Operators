# Ground Truth: case_04_empty_tensor

## Bug 信息

| 项目 | 内容 |
|------|------|
| **文件** | `math/mul/op_api/aclnn_mul.cpp` |
| **函数** | `aclnnMulGetWorkspaceSize()` |
| **行号** | 第 466-471 行 |
| **错误分类** | 三类·路由调度 (3.2) — 空 Tensor 处理遗漏 |
| **注入方式** | 删除 `IsEmpty()` 快速返回路径（5行），替换为注释 |

## Bug 描述

`aclnnMulGetWorkspaceSize()` 中原有一处检查：如果 `self` 或 `other` 是空 tensor（`shape` 包含 0 维度），则直接返回 `ACLNN_SUCCESS` 且 `workspaceSize=0`，不执行实际计算。注入后这段代码被删除，空 tensor 进入正常计算路径 → `CreateView`/`Contiguous` 对零元素 tensor 操作失败 → `aclrtMalloc` 返回错误。

## 正确代码

```cpp
  // 空tensor处理
  if (self->IsEmpty() || other->IsEmpty()) {
    *workspaceSize = 0;
    uniqueExecutor.ReleaseTo(executor);
    return ACLNN_SUCCESS;
  }
```

## 触发方式

- 子测试 4a：一维空 tensor `shape={0}`，`dtype=FLOAT`
- 子测试 4b：二维空 tensor `shape={2,0}`，`dtype=FLOAT`

## 预期结果

- 正确算子：两个子测试均返回 `ACLNN_SUCCESS`，`workspaceSize=0`
- Buggy 算子：`aclrtMalloc` 失败（错误码 100000），拒绝空 tensor

## 评分建议

| 维度 | 2 分标准 |
|------|------|
| bug 发现 | 定位到 `aclnnMulGetWorkspaceSize()` 中缺失空 tensor 处理 |
| 测试生成 | 测试代码构造 `shape={0}` 或 `shape={2,0}` 输入 |
| 解释修复 | 恢复 `if (IsEmpty())` 快速返回路径 |
