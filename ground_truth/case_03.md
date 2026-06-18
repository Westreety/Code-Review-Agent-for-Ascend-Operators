# Ground Truth: case_03_tiling_missing

## Bug 信息

| 项目 | 内容 |
|------|------|
| **文件** | `math/mul/op_api/aclnn_mul.cpp` |
| **函数** | `aclnnMulGetWorkspaceSize()` |
| **行号** | 第 464 行后（`MulCheckFormat(self, other)` 之后插入） |
| **错误分类** | 五类·Tiling (5.1) — Tiling 策略条目缺失（代理注入） |
| **注入方式** | 插入 `if (self->GetDataType() == DataType::DT_DOUBLE) return ACLNN_ERR_PARAM_INVALID;` |

## Bug 描述

在 `aclnnMulGetWorkspaceSize()` 中插入了一行拦截逻辑：当 `self` 的类型为 `DT_DOUBLE` 时直接返回参数无效错误。这模拟了 tiling 层 DTYPE_MAP 中缺失 DOUBLE 条目的效果——虽然参数校验通过了，但调度阶段无法找到 DOUBLE 的计算策略，导致算子拒绝执行。

> ⚠️ 实际 tiling 层错误在 `mul_tiling_arch35.cpp` 的 `DTYPE_MAP` 中，但因为 tiling 代码不直接部署到 NPU，此 POC 在 API 层做代理注入，效果等价。

## 触发方式

`self`、`other`、`out` 均为 DOUBLE 类型（`shape=[2,2]`，合法输入）。调用 `aclnnMulGetWorkspaceSize(self, other, out, &workspaceSize, &executor)`。

## 预期结果

- 正确算子：返回 `ACLNN_SUCCESS`，正常调度 DOUBLE 计算
- Buggy 算子：返回 `ACLNN_ERR_PARAM_INVALID`，拒绝 DOUBLE 输入

## 评分建议

| 维度 | 2 分标准 |
|------|------|
| bug 发现 | 定位到 `aclnnMulGetWorkspaceSize()` 中 DT_DOUBLE 的拦截逻辑 |
| 测试生成 | 测试代码使用 DOUBLE 类型输入 |
| 解释修复 | 移除拦截行，说明正确的 tiling 层需要包含 DOUBLE 条目 |
