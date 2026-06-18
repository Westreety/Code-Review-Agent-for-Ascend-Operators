# 注入说明

## 分类存疑

此 case 标注为 **2.1 混合精度组合被误删**，但实际注入行为更接近 **1.2 dtype白名单遗漏**。

| | 2.1 定义 | 实际注入 |
|------|------|------|
| 机制 | BF16-FLOAT 从 IsMulMixDtypeSupport() 移除 | DT_DOUBLE 在 GetWorkspaceSize 硬编码拒绝 |
| 表现 | 走 Cast 路径，性能退化 | 直接返回错误码，功能阻断 |
| 本质 | 优化路径缺失 | 合法 dtype 误杀 |

建议后续重新设计 2.1 的正确注入（修改 IsMulMixDtypeSupport），并将当前注入重标为 1.2。
