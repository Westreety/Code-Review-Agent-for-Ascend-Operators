# 3.2 空Tensor处理遗漏

- **错误分类**: 三类·路由调度 (3.2)
- **注入层**: op_api
- **注入位置**: `aclnn_mul.cpp` → `aclnnMulGetWorkspaceSize()` L466-472
- **注入难度**: 🟢 易
- **可检测性**: 🟢 易

## 注入内容

删除 `aclnnMulGetWorkspaceSize()` 中的空 tensor 提前返回代码块：
```cpp
// 删除以下代码块:
if (self->IsEmpty() || other->IsEmpty()) {
    *workspaceSize = 0;
    uniqueExecutor.ReleaseTo(executor);
    return ACLNN_SUCCESS;
}
```

空 tensor（shape={0}）走正常执行路径导致崩溃或未定义行为。
